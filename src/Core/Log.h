#pragma once

#include <filesystem>
#include <format>
#include <functional>
#include <mutex>
#include <string>
#include <cstdio>

namespace cbx
{

/** Global, thread-safe log. Lines go to an optional file and to an optional
	callback (used by the log window). The callback is invoked from worker
	threads, so it must not touch UI directly.
*/
class Log
{
public:
	using Callback = std::function<void(const std::string& line)>;

	static Log& Instance();

	void SetFile(const std::filesystem::path& path);	///< empty path = no file logging
	void SetCallback(Callback cb);
	void Write(std::string_view text);

private:
	Log() = default;
	~Log();
	std::mutex mutex;
	FILE* file = nullptr;
	Callback callback;
};

template <class... Args>
void LOG(std::format_string<Args...> fmt, Args&&... args)
{
	Log::Instance().Write(std::format(fmt, std::forward<Args>(args)...));
}

}	// namespace cbx
