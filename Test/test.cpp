#include "stdafx.h"
#include "CppUnitTest.h"

#pragma comment(lib, "avisynth.lib")

namespace bfs = boost::filesystem;

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

static const AVS_Linkage *AVS_linkage = nullptr;

namespace Test
{		
	TEST_CLASS(Test)
	{
	private:
		bfs::path pluginPath;
		std::unique_ptr<IScriptEnvironment> env;

		bfs::path GetBuildDir() {
			HMODULE handle;
			static wchar_t tmp;
			const auto err = GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, &tmp, &handle);
			if (!err)
				return "";
			wchar_t buffer[1024];
			const auto len = GetModuleFileNameW(handle, buffer, 1024);
			return bfs::path(std::wstring(buffer, len)).parent_path();
		}

	public:
		TEST_METHOD_INITIALIZE(setup)
		{
			pluginPath = GetBuildDir();
			if (pluginPath == "")
				Assert::Fail(L"Failed to determine the build folder.");
			pluginPath += bfs::path::preferred_separator;
			pluginPath += "AvsSource.dll";

			env = std::unique_ptr<IScriptEnvironment>(CreateScriptEnvironment());
			AVS_linkage = env->GetAVSLinkage();
		}

		TEST_METHOD(TestPluginNotLoaded)
		{
			auto exception = false;
			try {
				env->Invoke("AvsSource", "BlankClip()");
			}
			catch (...) {
				exception = true;
			}
			Assert::IsTrue(exception, L"Exception was not thrown.");
		}

		TEST_METHOD(TestBlankClip)
		{
			const auto ret1 = env->Invoke("LoadPlugin", pluginPath.string().c_str());
			const auto width = 773, height = 409, rate = 48017, channels = 7;
			const auto command = boost::format("BlankClip(width = %d, height = %d, audio_rate = %d, channels = %d)") % width % height % rate % channels;
			const auto ret2 = env->Invoke("AvsSource", command.str().c_str());
			const auto clip = ret2.AsClip();
			const auto info = clip->GetVideoInfo();
			Assert::AreEqual(width, info.width);
			Assert::AreEqual(height, info.height);
			Assert::AreEqual(rate, info.audio_samples_per_second);
			Assert::AreEqual(channels, info.nchannels);
			const auto frame = clip->GetFrame(11, env.get());
			Assert::AreEqual(height, frame->GetHeight());
			const auto samples = 3769;
			const auto size = static_cast<size_t>(info.BytesFromAudioSamples(samples));
			std::vector<char> buffer(size);
			clip->GetAudio(&buffer[0], 11117, samples, env.get());
		}

		TEST_METHOD(TestInception)
		{
			const auto ret1 = env->Invoke("LoadPlugin", pluginPath.string().c_str());
			const auto width = 1117, height = 647, rate = 44101, channels = 11;
			const auto command = boost::format("BlankClip(width = %d, height = %d, audio_rate = %d, channels = %d)") % width % height % rate % channels;
			const auto tmp = boost::format("LoadPlugin(\"%s\")\r\nAvsSource(\"%s\")") % pluginPath.string() % command.str();
			const auto ret2 = env->Invoke("AvsSource", tmp.str().c_str());
			const auto clip = ret2.AsClip();
			const auto info = clip->GetVideoInfo();
			Assert::AreEqual(width, info.width);
			Assert::AreEqual(height, info.height);
			Assert::AreEqual(rate, info.audio_samples_per_second);
			Assert::AreEqual(channels, info.nchannels);
			const auto frame = clip->GetFrame(17, env.get());
			Assert::AreEqual(height, frame->GetHeight());
			const auto samples = 4973;
			const auto size = static_cast<size_t>(info.BytesFromAudioSamples(samples));
			std::vector<char> buffer(size);
			clip->GetAudio(&buffer[0], 11117, samples, env.get());
		}
	};
}