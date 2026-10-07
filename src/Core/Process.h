#pragma once

#include <atomic>
#include <string>

namespace cbx
{

/** Run external process (hidden window) and wait for it to finish.
	\param abort optional flag; process is terminated when it becomes true
	\return process exit code or -1 if process could not be started
*/
int RunProcess(const std::wstring& commandLine, unsigned long priorityClass, const std::atomic<bool>* abort = nullptr);

/** Quote argument for command line if necessary */
std::wstring QuoteArg(const std::wstring& arg);

}	// namespace cbx
