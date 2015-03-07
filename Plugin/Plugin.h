#pragma once

namespace bp = boost::process;
namespace bpi = boost::process::initializers;

class Plugin : public IClip {
	std::mutex mutex;
	std::unique_ptr<PipeComm> pipeComm;
	IScriptEnvironment *env;
	VideoInfo video_info;

	void check_failure(int status);
	void check_failure(int status, const std::vector<char> &blob);

public:
	Plugin(IScriptEnvironment *env) : env(env) {}
	~Plugin();

	void initialize(bool is_script, const char *content);
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env);
	void __stdcall GetAudio(void *buf, __int64 start, __int64 count, IScriptEnvironment *env);
	bool __stdcall GetParity(int n);
	int __stdcall SetCacheHints(int cache_hints, int frame_range);
	const VideoInfo& __stdcall GetVideoInfo();
};
