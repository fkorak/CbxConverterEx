#include "Log.h"

#include <windows.h>

namespace cbx
{

Log& Log::Instance()
{
	static Log instance;
	return instance;
}

Log::~Log()
{
	if (file)
		fclose(file);
}

void Log::SetFile(const std::filesystem::path& path)
{
	std::lock_guard lock(mutex);
	if (file)
	{
		fclose(file);
		file = nullptr;
	}
	if (!path.empty())
		file = _wfsopen(path.c_str(), L"ab", _SH_DENYWR);
}

void Log::SetCallback(Callback cb)
{
	std::lock_guard lock(mutex);
	callback = std::move(cb);
}

void Log::Write(std::string_view text)
{
	SYSTEMTIME st;
	GetLocalTime(&st);
	std::string line = std::format("{:02}:{:02}:{:02}.{:03}  {}", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, text);

	std::lock_guard lock(mutex);
	if (file)
	{
		fprintf(file, "%04u-%02u-%02u %s\r\n", st.wYear, st.wMonth, st.wDay, line.c_str());
		fflush(file);
	}
	if (callback)
		callback(line);
#ifdef _DEBUG
	OutputDebugStringA((line + "\n").c_str());
#endif
}

}	// namespace cbx
