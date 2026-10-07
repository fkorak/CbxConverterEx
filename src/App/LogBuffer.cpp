#include "pch.h"
#include "LogBuffer.h"

namespace app
{

LogBuffer& LogBuffer::Get()
{
	static LogBuffer buffer;
	return buffer;
}

void LogBuffer::Add(const std::string& line)
{
	std::lock_guard lock(mutex);
	lines.push_back(line);
	nextSeq++;
	while (lines.size() > limit)
		lines.pop_front();
}

uint64_t LogBuffer::Fetch(uint64_t lastSeq, std::vector<std::string>& out)
{
	std::lock_guard lock(mutex);
	uint64_t firstSeq = nextSeq - lines.size();
	uint64_t from = std::max(lastSeq, firstSeq);
	for (uint64_t s = from; s < nextSeq; s++)
		out.push_back(lines[static_cast<size_t>(s - firstSeq)]);
	return nextSeq;
}

void LogBuffer::Clear()
{
	std::lock_guard lock(mutex);
	lines.clear();
}

void LogBuffer::SetLimit(size_t n)
{
	std::lock_guard lock(mutex);
	limit = std::max<size_t>(n, 10);
	while (lines.size() > limit)
		lines.pop_front();
}

}	// namespace app
