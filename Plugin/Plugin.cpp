#include "stdafx.h"
#include "Common.h"
#include "Plugin.h"

static const AVS_Linkage *AVS_linkage = nullptr;
static HINSTANCE g_instance;
static bfs::path g_host_path;

Plugin::~Plugin() {
	pipeComm->emit_int(REQUEST_QUIT);
}

void Plugin::check_failure(int status) {
	if (status)
		return;
	const auto blob = pipeComm->fetch_blob();
	check_failure(status, blob);
}

void Plugin::check_failure(int status, const std::vector<char> &blob) {
	if (status)
		return;
	const std::string tmp(blob.begin(), blob.end());
	env->ThrowError("AvsSource host failed: %s", tmp.c_str());
}

void Plugin::initialize(bool is_import, const char *content) {
	const auto p1 = bp::create_pipe();
	const auto p2 = bp::create_pipe();
	{
		bio::file_descriptor_source source(p1.source, bio::close_handle);
		bio::file_descriptor_sink sink(p2.sink, bio::close_handle);
		boost::system::error_code error;
		bp::execute(
			bpi::run_exe(g_host_path),
			bpi::bind_stdin(source),
			bpi::bind_stdout(sink),
			bpi::set_on_error(error));
		if (error.value())
			env->ThrowError("bp::execute failed: error = %d", error.value());
	}
	pipeComm = std::unique_ptr<PipeComm>(new PipeComm(p1.sink, p2.source, bio::close_handle));
		
	pipeComm->emit_int(is_import ? 1 : 0);
	pipeComm->emit_str(content);

	const auto status = pipeComm->fetch_int();
	if (status) {
		const auto length = pipeComm->fetch_int();
		assert(length == sizeof(video_info));
		pipeComm->fetch(&video_info, length);
	}
	else {
		const auto tmp = pipeComm->fetch_str();
		env->ThrowError("AvsSource host init failed: %s", tmp.c_str());
	}
}

PVideoFrame __stdcall Plugin::GetFrame(int n, IScriptEnvironment *env) {
	std::lock_guard<std::mutex> lock(mutex);
	pipeComm->emit_int(REQUEST_GET_FRAME);
	pipeComm->emit_int(n);
	const auto status = pipeComm->fetch_int();
	check_failure(status);
	const auto num_planes = pipeComm->fetch_int();
	const auto frame = env->NewVideoFrame(video_info);
	for (int i = 0; i < num_planes; i += 1) {
		const auto plane = pipeComm->fetch_int();
		const auto src_pitch = pipeComm->fetch_int();
		const auto dst_pitch = frame->GetPitch(plane);
		const auto src_row_size = pipeComm->fetch_int();
		const auto height = video_info.height >> video_info.GetPlaneHeightSubsampling(plane);
		const auto blob = pipeComm->fetch_blob();
		assert(blob.size() == height * src_pitch);
		auto src = &blob[0];
		auto dst = frame->GetWritePtr(plane);
		for (int i = 0; i < height; i += 1) {
			memcpy(dst, src, src_row_size);
			src += src_pitch;
			dst += dst_pitch;
		}
	}
	return frame;
}

void __stdcall Plugin::GetAudio(void *buf, __int64 start, __int64 count, IScriptEnvironment *env) {
	std::lock_guard<std::mutex> lock(mutex);
	pipeComm->emit_int(REQUEST_GET_AUDIO);
	pipeComm->emit_int64(start);
	pipeComm->emit_int64(count);
	const auto status = pipeComm->fetch_int();
	check_failure(status);
	const auto length = pipeComm->fetch_int();
	assert(length == video_info.BytesFromAudioSamples(count));
	pipeComm->fetch(buf, length);
}

bool __stdcall Plugin::GetParity(int n) {
	std::lock_guard<std::mutex> lock(mutex);
	pipeComm->emit_int(REQUEST_GET_PARITY);
	pipeComm->emit_int(n);
	const auto status = pipeComm->fetch_int();
	check_failure(status);
	const auto ret = pipeComm->fetch_int();
	return ret != 0;
}

int __stdcall Plugin::SetCacheHints(int cache_hints, int frame_range) {
	std::lock_guard<std::mutex> lock(mutex);
	pipeComm->emit_int(REQUEST_SET_CACHE_HINTS);
	pipeComm->emit_int(cache_hints);
	pipeComm->emit_int(frame_range);
	const auto status = pipeComm->fetch_int();
	check_failure(status);
	const auto ret = pipeComm->fetch_int();
	return ret;
}

const VideoInfo& __stdcall Plugin::GetVideoInfo() {
	return video_info;
}

static AVSValue new_avs_source(bool is_import, AVSValue args, IScriptEnvironment *env) {
	const auto result = new Plugin(env);
	result->initialize(is_import, args[0].AsString());
	return result;
}

static AVSValue __cdecl AvsEvalSourceFunction(AVSValue args, void *user_data, IScriptEnvironment *env) {
	return new_avs_source(false, args, env);
}

static AVSValue __cdecl AvsImportSourceFunction(AVSValue args, void *user_data, IScriptEnvironment *env) {
	return new_avs_source(true, args, env);
}

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment *env, const AVS_Linkage* const vectors) {
	AVS_linkage = vectors;
	env->AddFunction("AvsSource", "s", AvsEvalSourceFunction, nullptr);
	env->AddFunction("AvsImportSource", "s", AvsImportSourceFunction, nullptr);
	return "AvsSource plugin";
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (g_instance)
		return TRUE;
	g_instance = hinstDLL;
	wchar_t buffer[1024];
	const auto len = GetModuleFileNameW(hinstDLL, buffer, 1024);
	if (len) {
		g_host_path = bfs::path(std::wstring(buffer, len)).parent_path();
		g_host_path += bfs::path::preferred_separator;
	}
	g_host_path += "AvsSource.exe";
	return TRUE;
}