#pragma once

#include <atomic>
#include <filesystem>
#include <string>

namespace cbx
{

/** Extract archive (zip/rar/rar5/7z/tar/...) to directory using libarchive.
	Entries with unsafe paths (absolute, "..") are skipped.
*/
bool ExtractArchive(const std::filesystem::path& archive, const std::filesystem::path& destDir,
	const std::wstring& password, const std::atomic<bool>& abort, std::string& error);

/** Extract archive with external 7z.exe (fallback, e.g. for encrypted rar) */
bool ExtractArchive7z(const std::filesystem::path& sevenZip, const std::filesystem::path& archive,
	const std::filesystem::path& destDir, const std::wstring& password, unsigned long priorityClass,
	const std::atomic<bool>& abort, std::string& error);

/** Locate 7z.exe: configured path, application directory or 7-Zip installation */
std::filesystem::path Find7z(const std::wstring& configured);

/** libarchive version details, for about box */
std::string ArchiveLibraryVersion();

/** Pack directory content (recursively) into zip file without compression */
bool CreateZip(const std::filesystem::path& zipFile, const std::filesystem::path& srcDir,
	const std::atomic<bool>& abort, std::string& error);

}	// namespace cbx
