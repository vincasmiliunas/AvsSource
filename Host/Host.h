#pragma once

/* Workaround to avoid breaking PClip's destructor after assignment. */
/* Instead use copy constructor for transferring PClip's value. */
class PClipContainer {
	PClip pclip;
public:
	PClipContainer(PClip pclip) : pclip(pclip) {}
	~PClipContainer() = default;
	PClip &get() { return pclip; }
};

class Host {
	std::string id;
	std::unique_ptr<PipeComm> pipeComm;
	std::unique_ptr<IScriptEnvironment> env;
	std::unique_ptr<PClipContainer> clip;
	std::vector<char> audio;

public:
	void initialize();
	bool communicate();

	void emit_failure(const boost::format &content);
	void emit_failure(const std::string &content);
};
