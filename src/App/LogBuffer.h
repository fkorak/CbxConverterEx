#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace app
{

/** Collects log lines from any thread, limited to configured number of lines.
	Log window polls it for new lines.
*/
class LogBuffer
{
public:
	static LogBuffer& Get();

	void Add(const std::string& line);
	/** Get lines added after sequence number lastSeq (0 = all).
		\return sequence number to use for next call
	*/
	uint64_t Fetch(uint64_t lastSeq, std::vector<std::string>& out);
	void Clear();
	void SetLimit(size_t lines);

private:
	std::mutex mutex;
	std::deque<std::string> lines;
	uint64_t nextSeq = 1;	///< sequence number of next added line
	size_t limit = 1000;
};

}	// namespace app
