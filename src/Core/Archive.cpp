#include "Archive.h"
#include "Process.h"
#include "StringUtils.h"

#include <archive.h>
#include <archive_entry.h>

#include <windows.h>
#include <algorithm>
#include <memory>
#include <vector>

namespace fs = std::filesystem;

namespace cbx
{

namespace
{

struct ReadArchiveDeleter
{
	void operator()(archive* a) const
	{
		archive_read_free(a);
	}
};
struct WriteArchiveDeleter
{
	void operator()(archive* a) const
	{
		archive_write_free(a);
	}
};

/** Build safe relative path from archive entry name; empty if entry must be skipped */
fs::path SanitizeEntryPath(const std::wstring& name)
{
	fs::path rel;
	size_t start = 0;
	while (start <= name.size())
	{
		size_t end = name.find_first_of(L"/\\", start);
		if (end == std::wstring::npos)
			end = name.size();
		std::wstring part = name.substr(start, end - start);
		start = end + 1;
		if (part.empty() || part == L".")
			continue;
		if (part == L".." || part.find(L':') != std::wstring::npos)
			return {};
		rel /= part;
	}
	return rel;
}

std::wstring EntryName(archive_entry* entry)
{
	if (const char* utf8 = archive_entry_pathname_utf8(entry))
		return FromUtf8(utf8);
	if (const wchar_t* w = archive_entry_pathname_w(entry))
		return w;
	if (const char* mbs = archive_entry_pathname(entry))
	{
		int len = MultiByteToWideChar(CP_ACP, 0, mbs, -1, nullptr, 0);
		std::wstring out(len > 0 ? len - 1 : 0, L'\0');
		MultiByteToWideChar(CP_ACP, 0, mbs, -1, out.data(), len);
		return out;
	}
	return {};
}

enum class ExtractResult
{
	Ok,
	Failed,
	NameNotConvertible	///< entry name not valid in requested header charset
};

/** \param charset charset of entry names that are not marked as Unicode (zip, rar4, tar ...)
	\param strictNames stop with NameNotConvertible if a name cannot be converted
*/
ExtractResult ExtractWithCharset(const fs::path& archivePath, const fs::path& destDir,
	const std::wstring& password, const std::atomic<bool>& abort, std::string& error,
	const std::string& charset, bool strictNames)
{
	std::unique_ptr<archive, ReadArchiveDeleter> a(archive_read_new());
	archive_read_support_filter_all(a.get());
	archive_read_support_format_all(a.get());
	archive_read_set_options(a.get(), ("hdrcharset=" + charset).c_str());
	if (!password.empty())
		archive_read_add_passphrase(a.get(), ToUtf8(password).c_str());
	if (archive_read_open_filename_w(a.get(), archivePath.c_str(), 1 << 20) != ARCHIVE_OK)
	{
		error = archive_error_string(a.get()) ? archive_error_string(a.get()) : "cannot open archive";
		return ExtractResult::Failed;
	}

	std::vector<char> buffer(1 << 20);
	archive_entry* entry;
	int files = 0;
	for (;;)
	{
		int r = archive_read_next_header(a.get(), &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r < ARCHIVE_WARN)
		{
			error = archive_error_string(a.get()) ? archive_error_string(a.get()) : "error reading archive";
			return ExtractResult::Failed;
		}
		if (r == ARCHIVE_WARN && strictNames)
		{
			// typically "Pathname cannot be converted from <charset>"
			error = archive_error_string(a.get()) ? archive_error_string(a.get()) : "entry name cannot be converted";
			return ExtractResult::NameNotConvertible;
		}
		if (abort)
		{
			error = "aborted";
			return ExtractResult::Failed;
		}
		if (archive_entry_filetype(entry) != AE_IFREG)
			continue;
		fs::path rel = SanitizeEntryPath(EntryName(entry));
		if (rel.empty())
			continue;
		fs::path out = destDir / rel;
		std::error_code ec;
		fs::create_directories(out.parent_path(), ec);

		HANDLE h = CreateFileW(out.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
		{
			error = "cannot create " + ToUtf8(out.native());
			return ExtractResult::Failed;
		}
		bool ok = true;
		for (;;)
		{
			la_ssize_t n = archive_read_data(a.get(), buffer.data(), buffer.size());
			if (n == 0)
				break;
			if (n < 0)
			{
				error = archive_error_string(a.get()) ? archive_error_string(a.get()) : "error reading archive data";
				ok = false;
				break;
			}
			DWORD written;
			if (!WriteFile(h, buffer.data(), static_cast<DWORD>(n), &written, nullptr) || written != static_cast<DWORD>(n))
			{
				error = "cannot write " + ToUtf8(out.native());
				ok = false;
				break;
			}
		}
		CloseHandle(h);
		if (!ok)
			return ExtractResult::Failed;
		files++;
	}
	if (files == 0)
	{
		error = "archive contains no files";
		return ExtractResult::Failed;
	}
	return ExtractResult::Ok;
}

}	// namespace

bool ExtractArchive(const fs::path& archivePath, const fs::path& destDir,
	const std::wstring& password, const std::atomic<bool>& abort, std::string& error)
{
	// Entry names not marked as Unicode (zip, rar4) are stored in the charset of the system that created
	// the archive: UTF-8 on Linux/macOS, DOS code page on Windows. Like 7-Zip, try UTF-8 first and
	// fall back to the OEM code page if any name is not valid UTF-8.
	ExtractResult r = ExtractWithCharset(archivePath, destDir, password, abort, error, "UTF-8", true);
	if (r != ExtractResult::NameNotConvertible)
		return r == ExtractResult::Ok;

	std::error_code ec;
	fs::remove_all(destDir, ec);
	fs::create_directories(destDir, ec);
	error.clear();
	return ExtractWithCharset(archivePath, destDir, password, abort, error, "CP" + std::to_string(GetOEMCP()), false) == ExtractResult::Ok;
}

bool ExtractArchive7z(const fs::path& sevenZip, const fs::path& archivePath,
	const fs::path& destDir, const std::wstring& password, unsigned long priorityClass,
	const std::atomic<bool>& abort, std::string& error)
{
	std::wstring cmd = QuoteArg(sevenZip.native()) + L" x " + QuoteArg(L"-p" + password) + L" " +
		QuoteArg(archivePath.native()) + L" -y " + QuoteArg(L"-o" + destDir.native());
	int rc = RunProcess(cmd, priorityClass, &abort);
	if (rc != 0)
	{
		error = "7z.exe failed with exit code " + std::to_string(rc);
		return false;
	}
	return true;
}

fs::path Find7z(const std::wstring& configured)
{
	std::error_code ec;
	if (!configured.empty() && fs::exists(configured, ec))
		return configured;

	wchar_t exe[MAX_PATH];
	GetModuleFileNameW(nullptr, exe, MAX_PATH);
	fs::path local = fs::path(exe).parent_path() / L"7z.exe";
	if (fs::exists(local, ec))
		return local;

	for (HKEY root : { HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER })
	{
		wchar_t buf[MAX_PATH];
		DWORD size = sizeof(buf);
		if (RegGetValueW(root, L"SOFTWARE\\7-Zip", L"Path", RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS)
		{
			fs::path p = fs::path(buf) / L"7z.exe";
			if (fs::exists(p, ec))
				return p;
		}
	}
	wchar_t pf[MAX_PATH];
	if (GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH))
	{
		fs::path p = fs::path(pf) / L"7-Zip" / L"7z.exe";
		if (fs::exists(p, ec))
			return p;
	}
	return {};
}

std::string ArchiveLibraryVersion()
{
	return archive_version_details();
}

bool CreateZip(const fs::path& zipFile, const fs::path& srcDir, const std::atomic<bool>& abort, std::string& error)
{
	std::vector<fs::path> files;
	std::error_code ec;
	for (auto it = fs::recursive_directory_iterator(srcDir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
	{
		if (it->is_regular_file(ec))
			files.push_back(it->path());
	}
	if (ec)
	{
		error = "cannot list " + ToUtf8(srcDir.native()) + ": " + ec.message();
		return false;
	}
	std::sort(files.begin(), files.end());

	fs::path tmp = zipFile;
	tmp += L".tmp";

	std::unique_ptr<archive, WriteArchiveDeleter> a(archive_write_new());
	archive_write_set_format_zip(a.get());
	archive_write_set_options(a.get(), "zip:compression=store,zip:hdrcharset=UTF-8");
	if (archive_write_open_filename_w(a.get(), tmp.c_str()) != ARCHIVE_OK)
	{
		error = archive_error_string(a.get()) ? archive_error_string(a.get()) : "cannot create output file";
		return false;
	}

	std::vector<char> buffer(1 << 20);
	bool ok = true;
	for (const auto& file : files)
	{
		if (abort)
		{
			error = "aborted";
			ok = false;
			break;
		}
		std::string rel = ToUtf8(fs::relative(file, srcDir, ec).generic_wstring());
		HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (h == INVALID_HANDLE_VALUE)
		{
			error = "cannot open " + ToUtf8(file.native());
			ok = false;
			break;
		}
		LARGE_INTEGER size{};
		FILETIME mtime{};
		GetFileSizeEx(h, &size);
		GetFileTime(h, nullptr, nullptr, &mtime);
		ULARGE_INTEGER t;
		t.LowPart = mtime.dwLowDateTime;
		t.HighPart = mtime.dwHighDateTime;
		time_t unixTime = static_cast<time_t>((t.QuadPart - 116444736000000000ULL) / 10000000ULL);

		archive_entry* entry = archive_entry_new();
		archive_entry_set_pathname_utf8(entry, rel.c_str());
		archive_entry_set_size(entry, size.QuadPart);
		archive_entry_set_filetype(entry, AE_IFREG);
		archive_entry_set_perm(entry, 0644);
		archive_entry_set_mtime(entry, unixTime, 0);
		if (archive_write_header(a.get(), entry) != ARCHIVE_OK)
		{
			error = archive_error_string(a.get()) ? archive_error_string(a.get()) : "cannot write zip entry";
			ok = false;
		}
		while (ok)
		{
			DWORD read = 0;
			if (!ReadFile(h, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr))
			{
				error = "cannot read " + ToUtf8(file.native());
				ok = false;
				break;
			}
			if (read == 0)
				break;
			if (archive_write_data(a.get(), buffer.data(), read) < 0)
			{
				error = archive_error_string(a.get()) ? archive_error_string(a.get()) : "cannot write zip data";
				ok = false;
			}
		}
		archive_entry_free(entry);
		CloseHandle(h);
		if (!ok)
			break;
	}
	if (archive_write_close(a.get()) != ARCHIVE_OK && ok)
	{
		error = archive_error_string(a.get()) ? archive_error_string(a.get()) : "cannot finalize zip";
		ok = false;
	}
	a.reset();
	if (!ok)
	{
		DeleteFileW(tmp.c_str());
		return false;
	}
	if (!MoveFileExW(tmp.c_str(), zipFile.c_str(), MOVEFILE_REPLACE_EXISTING))
	{
		error = "cannot rename " + ToUtf8(tmp.native());
		DeleteFileW(tmp.c_str());
		return false;
	}
	return true;
}

}	// namespace cbx
