#include "Process.h"
#include "Log.h"
#include "StringUtils.h"

#include <windows.h>

namespace cbx
{

int RunProcess(const std::wstring& commandLine, unsigned long priorityClass, const std::atomic<bool>* abort)
{
	STARTUPINFOW si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi{};

	std::wstring cmd = commandLine;	// CreateProcessW may modify buffer
	if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, priorityClass | CREATE_NO_WINDOW,
		nullptr, nullptr, &si, &pi))
	{
		LOG("CreateProcess failed ({}): {}", GetLastError(), ToUtf8(commandLine));
		return -1;
	}
	CloseHandle(pi.hThread);

	for (;;)
	{
		DWORD rc = WaitForSingleObject(pi.hProcess, 200);
		if (rc != WAIT_TIMEOUT)
			break;
		if (abort && *abort)
		{
			TerminateProcess(pi.hProcess, 1);
			WaitForSingleObject(pi.hProcess, 5000);
			break;
		}
	}

	DWORD exitCode = static_cast<DWORD>(-1);
	if (!GetExitCodeProcess(pi.hProcess, &exitCode))
		exitCode = static_cast<DWORD>(-1);
	CloseHandle(pi.hProcess);
	return static_cast<int>(exitCode);
}

std::wstring QuoteArg(const std::wstring& arg)
{
	if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos)
		return arg;
	std::wstring out = L"\"";
	for (size_t i = 0; i < arg.size(); i++)
	{
		size_t backslashes = 0;
		while (i < arg.size() && arg[i] == L'\\')
		{
			i++;
			backslashes++;
		}
		if (i == arg.size())
		{
			out.append(backslashes * 2, L'\\');
			break;
		}
		if (arg[i] == L'"')
			out.append(backslashes * 2 + 1, L'\\');
		else
			out.append(backslashes, L'\\');
		out.push_back(arg[i]);
	}
	out.push_back(L'"');
	return out;
}

}	// namespace cbx
