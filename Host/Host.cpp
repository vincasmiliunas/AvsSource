#include "stdafx.h"
#include "Common.h"
#include "Host.h"

#pragma comment(lib, "avisynth.lib")

static const AVS_Linkage *AVS_linkage = nullptr;

void Host::initialize() {
	const auto unique = boost::format("AvsSource-%s") % bfs::unique_path().string();
	id = unique.str();

	const auto std_output = GetStdHandle(STD_OUTPUT_HANDLE);
	if (!std_output || std_output == INVALID_HANDLE_VALUE)
		throw new std::runtime_error("GetStdHandle(STD_OUTPUT_HANDLE) failed.");
	const auto std_input = GetStdHandle(STD_INPUT_HANDLE);
	if (!std_input || std_input == INVALID_HANDLE_VALUE)
		throw new std::runtime_error("GetStdHandle(STD_INPUT_HANDLE) failed.");
	pipeComm = std::unique_ptr<PipeComm>(new PipeComm(std_output, std_input, bio::never_close_handle));

	const auto is_import = pipeComm->fetch_int() != 0;
	const auto source = pipeComm->fetch_str();

	env = std::unique_ptr<IScriptEnvironment>(CreateScriptEnvironment());
	AVS_linkage = env->GetAVSLinkage();
	const auto ret = env->Invoke(is_import ? "Import" : "Eval", source.c_str());
	clip = std::unique_ptr<PClipContainer>(new PClipContainer(ret.AsClip()));
	const auto info = clip->get()->GetVideoInfo();

	pipeComm->emit_int(1);
	pipeComm->emit_blob(&info, sizeof(info));
}

bool Host::communicate() {
	const auto cmd = pipeComm->fetch_int();
	switch (cmd) {
	case REQUEST_QUIT:
		return false;
	case REQUEST_GET_FRAME:
	{
		const auto info = clip->get()->GetVideoInfo();
		const auto index = pipeComm->fetch_int();
		const auto frame = clip->get()->GetFrame(index, env.get());

		const auto emit_plane = [&](int plane) {
			const auto height = info.height >> info.GetPlaneHeightSubsampling(plane);
			const auto pitch = frame->GetPitch(plane);
			pipeComm->emit_int(plane);
			pipeComm->emit_int(pitch);
			pipeComm->emit_int(frame->GetRowSize(plane));
			pipeComm->emit_blob(frame->GetReadPtr(plane), height * pitch);
		};

		pipeComm->emit_int(1);
		pipeComm->emit_int(info.IsPlanar() ? 3 : 1);
		emit_plane(PLANAR_Y);
		if (info.IsPlanar()) {
			emit_plane(PLANAR_U);
			emit_plane(PLANAR_V);
		}

		return true;
	}
	case REQUEST_GET_AUDIO:
	{
		const auto start = pipeComm->fetch_int64();
		const auto count = pipeComm->fetch_int64();
		const auto size = clip->get()->GetVideoInfo().BytesFromAudioSamples(count);
		if (size > audio.capacity())
			audio.resize(static_cast<unsigned int>(size));
		const auto ptr = &audio[0];
		clip->get()->GetAudio(ptr, start, count, env.get());
		pipeComm->emit_int(1);
		pipeComm->emit_blob(ptr, static_cast<size_t>(size));
		return true;
	}
	case REQUEST_GET_PARITY:
	{
		const auto index = pipeComm->fetch_int();
		const auto ret = clip->get()->GetParity(index);
		pipeComm->emit_int(1);
		pipeComm->emit_int(ret);
		return true;
	}
	case REQUEST_SET_CACHE_HINTS:
	{
		const auto cache_hints = pipeComm->fetch_int();
		const auto frame_range = pipeComm->fetch_int();
		const auto ret = clip->get()->SetCacheHints(cache_hints, frame_range);
		pipeComm->emit_int(1);
		pipeComm->emit_int(ret);
		return true;
	}
	default:
	{
		const auto tmp = boost::format("Unknown command: %d") % cmd;
		throw new std::invalid_argument(tmp.str());
	}
	}
}

void Host::emit_failure(const boost::format &content) {
	emit_failure(content.str());
}

void Host::emit_failure(const std::string &content) {
	pipeComm->emit_int(0);
	pipeComm->emit_str(content);
}

int __stdcall WinMain(HINSTANCE, HINSTANCE, char *, int) {
	Host host;

	try {
		host.initialize();
		while (host.communicate())
			;
	}
	catch (const std::exception &ex) {
		host.emit_failure(boost::format("std::exception: %s") % ex.what());
	}
	catch (const IScriptEnvironment::NotFound &ex) {
		(void)ex;
		host.emit_failure("IScriptEnvironment::NotFound");
	}
	catch (const AvisynthError &ex) {
		host.emit_failure(boost::format("AvisynthError: %s") % ex.msg);
	}
	catch (...) {
		host.emit_failure("Unknown exception.");
	}

	return 0;
}