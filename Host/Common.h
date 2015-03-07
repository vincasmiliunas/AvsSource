#pragma once

namespace bio = boost::iostreams;
namespace bfs = boost::filesystem;

enum request_t {
	REQUEST_NONE,
	REQUEST_QUIT,
	REQUEST_GET_FRAME,
	REQUEST_GET_AUDIO,
	REQUEST_GET_PARITY,
	REQUEST_SET_CACHE_HINTS
};

class PipeComm {
	bio::file_descriptor_sink sink;
	bio::file_descriptor_source source;
	bio::stream<bio::file_descriptor_sink> output;
	bio::stream<bio::file_descriptor_source> input;

public:
	PipeComm(HANDLE handle_sink, HANDLE handle_source, enum bio::file_descriptor_flags flags) :
		sink(handle_sink, flags), source(handle_source, flags), output(sink), input(source) {
	}
	~PipeComm() = default;

	void emit_int(int value) {
		emit(&value, sizeof(int));
	}

	void emit_int64(__int64 value) {
		emit(&value, sizeof(__int64));
	}

	void emit(const void *ptr, size_t length) {
		output.write(static_cast<const char *>(ptr), length);
		output.flush();
	}

	void emit_blob(const void *ptr, size_t length) {
		emit_int(static_cast<int>(length));
		emit(ptr, length);
	}

	void emit_str(const std::string &value) {
		emit_blob(value.c_str(), value.length());
	}

	void fetch(void *ptr, size_t length) {
		input.read(static_cast<char *>(ptr), length);
	}

	int fetch_int() {
		int value;
		fetch(&value, sizeof(int));
		return value;
	}

	__int64 fetch_int64() {
		__int64 value;
		fetch(&value, sizeof(__int64));
		return value;
	}

	const std::vector<char> fetch_blob() {
		const auto length = fetch_int();
		std::vector<char> blob(length);
		fetch(&blob[0], length);
		return blob;
	}

	const std::string fetch_str() {
		const auto blob = fetch_blob();
		return std::string(blob.begin(), blob.end());
	}
};
