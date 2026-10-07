#include "Settings.h"
#include "StringUtils.h"

#include <windows.h>
#include <algorithm>
#include <format>
#include <thread>

namespace cbx
{

namespace
{

class Ini
{
public:
	explicit Ini(const std::filesystem::path& path) : path(path.wstring()) {}

	std::wstring ReadString(const wchar_t* section, const wchar_t* key, const std::wstring& def) const
	{
		std::wstring buf(4096, L'\0');
		DWORD len = GetPrivateProfileStringW(section, key, def.c_str(), buf.data(), static_cast<DWORD>(buf.size()), path.c_str());
		buf.resize(len);
		return buf;
	}
	bool HasKey(const wchar_t* section, const wchar_t* key) const
	{
		wchar_t buf[8];
		static const wchar_t* marker = L"\x01\x02";
		GetPrivateProfileStringW(section, key, marker, buf, 8, path.c_str());
		return wcscmp(buf, marker) != 0;
	}
	int ReadInt(const wchar_t* section, const wchar_t* key, int def) const
	{
		std::wstring s = ReadString(section, key, L"");
		if (s.empty())
			return def;
		try
		{
			return std::stoi(s);
		}
		catch (...)
		{
			return def;
		}
	}
	bool ReadBool(const wchar_t* section, const wchar_t* key, bool def) const
	{
		return ReadInt(section, key, def ? 1 : 0) != 0;
	}

	bool WriteString(const wchar_t* section, const wchar_t* key, const std::wstring& val) const
	{
		return WritePrivateProfileStringW(section, key, val.c_str(), path.c_str()) != 0;
	}
	bool WriteInt(const wchar_t* section, const wchar_t* key, int val) const
	{
		return WriteString(section, key, std::to_wstring(val));
	}
	bool WriteBool(const wchar_t* section, const wchar_t* key, bool val) const
	{
		return WriteInt(section, key, val ? 1 : 0);
	}

private:
	std::wstring path;
};

// Windows priority class values used by the original version
enum
{
	LEGACY_IDLE_PRIORITY_CLASS = 0x40,
	LEGACY_BELOW_NORMAL_PRIORITY_CLASS = 0x4000,
	LEGACY_NORMAL_PRIORITY_CLASS = 0x20,
	LEGACY_ABOVE_NORMAL_PRIORITY_CLASS = 0x8000
};

Settings::Worker::Priority PriorityFromLegacy(int value)
{
	switch (value)
	{
	case LEGACY_IDLE_PRIORITY_CLASS:
		return Settings::Worker::PriorityIdle;
	case LEGACY_NORMAL_PRIORITY_CLASS:
		return Settings::Worker::PriorityNormal;
	case LEGACY_ABOVE_NORMAL_PRIORITY_CLASS:
		return Settings::Worker::PriorityAboveNormal;
	default:
		return Settings::Worker::PriorityBelowNormal;
	}
}

int PriorityToLegacy(Settings::Worker::Priority p)
{
	switch (p)
	{
	case Settings::Worker::PriorityIdle:
		return LEGACY_IDLE_PRIORITY_CLASS;
	case Settings::Worker::PriorityNormal:
		return LEGACY_NORMAL_PRIORITY_CLASS;
	case Settings::Worker::PriorityAboveNormal:
		return LEGACY_ABOVE_NORMAL_PRIORITY_CLASS;
	default:
		return LEGACY_BELOW_NORMAL_PRIORITY_CLASS;
	}
}

/** Extract "-quality N" from ImageMagick parameters used by the original version */
int QualityFromImParams(const std::wstring& params, int def)
{
	size_t pos = params.find(L"-quality");
	if (pos == std::wstring::npos)
		return def;
	try
	{
		return std::stoi(params.substr(pos + 8));
	}
	catch (...)
	{
		return def;
	}
}

}	// namespace

int Settings::Worker::DefaultThreadCount()
{
	unsigned int n = std::thread::hardware_concurrency();
	return std::clamp<int>(n ? n : 1, THREAD_COUNT_MIN, THREAD_COUNT_MAX);
}

const wchar_t* Settings::Conversion::FormatExtension(Format f)
{
	switch (f)
	{
	case FormatJpeg:
		return L".jpg";
	case FormatPng:
		return L".png";
	default:
		return L".webp";
	}
}

const wchar_t* Settings::Conversion::FormatName(Format f)
{
	switch (f)
	{
	case FormatJpeg:
		return L"JPEG";
	case FormatPng:
		return L"PNG (lossless)";
	default:
		return L"WebP";
	}
}

const wchar_t* Settings::Directories::DirectoryTypeDescription(DirectoryType type)
{
	switch (type)
	{
	case DirectoryTypeDefault:
		return L"default (relative subdirectory)";
	case DirectoryTypeCustom:
		return L"custom";
	default:
		return L"???";
	}
}

int Settings::GetDefaultResize(int width) const
{
	if (width > 0)
	{
		for (const auto& r : conversion.defResize)
		{
			if (r.width == width && r.resize > 0)
				return r.resize;
		}
	}
	return 100;
}

bool Settings::Read(const std::filesystem::path& fileName)
{
	*this = Settings();
	worker.threadCount = Worker::DefaultThreadCount();
	if (!std::filesystem::exists(fileName))
		return false;

	Ini ini(fileName);

	int maxX = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int maxY = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	mainWindow.width = ini.ReadInt(L"frmMain", L"AppWidth", mainWindow.width);
	mainWindow.height = ini.ReadInt(L"frmMain", L"AppHeight", mainWindow.height);
	mainWindow.width = std::clamp(mainWindow.width, 400, std::max(400, maxX));
	mainWindow.height = std::clamp(mainWindow.height, 250, std::max(250, maxY));
	mainWindow.posX = ini.ReadInt(L"frmMain", L"AppPositionX", mainWindow.posX);
	mainWindow.posY = ini.ReadInt(L"frmMain", L"AppPositionY", mainWindow.posY);
	mainWindow.maximized = ini.ReadBool(L"frmMain", L"Maximized", false);
	mainWindow.alwaysOnTop = ini.ReadBool(L"frmMain", L"AlwaysOnTop", false);
	mainWindow.columnWidths = ini.ReadString(L"frmMain", L"ColumnWidths", L"");

	logging.logToFile = ini.ReadBool(L"Logging", L"LogToFile", false);
	logging.maxUiLogLines = ini.ReadInt(L"Logging", L"MaxUiLogLines", 1000);

	worker.priority = PriorityFromLegacy(ini.ReadInt(L"Worker", L"Priority", LEGACY_BELOW_NORMAL_PRIORITY_CLASS));
	worker.playSoundWhenDone = ini.ReadBool(L"Worker", L"PlaySoundWhenDone", true);
	worker.threadCount = ini.ReadInt(L"Worker", L"ThreadCount", worker.threadCount);
	if (worker.threadCount < Worker::THREAD_COUNT_MIN || worker.threadCount > Worker::THREAD_COUNT_MAX)
		worker.threadCount = Worker::DefaultThreadCount();

	for (size_t i = 0; i < conversion.defResize.size(); i++)
	{
		auto& r = conversion.defResize[i];
		r.width = ini.ReadInt(L"Conversion", std::format(L"DefaultResizeWidth{}", i).c_str(), 0);
		r.resize = ini.ReadInt(L"Conversion", std::format(L"DefaultResizePct{}", i).c_str(), 100);
		if (r.resize <= 0 || r.resize >= 1000)
			r.resize = 0;
	}

	std::wstring ext = ToLower(Trim(ini.ReadString(L"Conversion", L"OutputExtension", L"webp")));
	if (!ext.empty() && ext[0] == L'.')
		ext.erase(0, 1);
	if (ext == L"jpg" || ext == L"jpeg")
		conversion.format = Conversion::FormatJpeg;
	else if (ext == L"png")
		conversion.format = Conversion::FormatPng;
	else
		conversion.format = Conversion::FormatWebp;

	if (ini.HasKey(L"Conversion", L"Quality"))
		conversion.quality = ini.ReadInt(L"Conversion", L"Quality", conversion.quality);
	else	// migrate from ImageMagick parameters
		conversion.quality = QualityFromImParams(ini.ReadString(L"Conversion", L"ImExtraParams", L"-quality 75"), conversion.quality);
	conversion.quality = std::clamp(conversion.quality, 1, 100);
	conversion.webpMethod = std::clamp(ini.ReadInt(L"Conversion", L"WebpMethod", conversion.webpMethod), 0, 6);
	conversion.webpLossless = ini.ReadBool(L"Conversion", L"WebpLossless", conversion.webpLossless);
	conversion.keepOriginalIfSmaller = ini.ReadBool(L"Conversion", L"KeepOriginalIfSmaller", conversion.keepOriginalIfSmaller);
	conversion.filesToSkip = ini.ReadString(L"Conversion", L"FilesToSkip", L"");
	conversion.unpackPassword = ini.ReadString(L"Conversion", L"UnpackPassword", L"");
	conversion.copyFileToOutputOnError = ini.ReadBool(L"Conversion", L"copyFileToOutputOnError", conversion.copyFileToOutputOnError);

	tools.sevenZipLocation = ini.ReadString(L"Tools", L"SevenZipLocation", tools.sevenZipLocation);

	pdfImport.gsLocation = ini.ReadString(L"PdfImport", L"GsLocation", pdfImport.gsLocation);
	pdfImport.gsParams = ini.ReadString(L"PdfImport", L"GsParams", pdfImport.gsParams);
	pdfImport.gsFilePattern = ini.ReadString(L"PdfImport", L"GsFilePattern", pdfImport.gsFilePattern);

	auto readDirType = [&](const wchar_t* key, Directories::DirectoryType def) {
		int v = ini.ReadInt(L"Directories", key, def);
		if (v < 0 || v >= Directories::DirectoryTypeLimiter)
			return Directories::DirectoryTypeDefault;
		return static_cast<Directories::DirectoryType>(v);
	};
	directories.tmpDirectoryType = readDirType(L"TmpDirectoryType", directories.tmpDirectoryType);
	directories.customTmpDirectory = ini.ReadString(L"Directories", L"CustomTmpDirectory", directories.customTmpDirectory);
	directories.outDirectoryType = readDirType(L"OutDirectoryType", directories.outDirectoryType);
	directories.customOutDirectory = ini.ReadString(L"Directories", L"CustomOutDirectory", directories.customOutDirectory);
	directories.useSourceDirectoryForOutput = ini.ReadBool(L"Directories", L"UseSourceDirectoryForOutput", directories.useSourceDirectoryForOutput);
	directories.recreateSourceDirectoryForOutput = ini.ReadBool(L"Directories", L"RecreateSourceDirectoryForOutput", directories.recreateSourceDirectoryForOutput);
	directories.useNumericTmpFileDirectory = ini.ReadBool(L"Directories", L"UseNumericTmpFileDirectory", directories.useNumericTmpFileDirectory);

	return true;
}

bool Settings::Write(const std::filesystem::path& fileName) const
{
	if (!std::filesystem::exists(fileName))
	{
		// UTF-16 BOM makes the profile API store non-ASCII paths without loss
		if (FILE* f = _wfopen(fileName.c_str(), L"wb"))
		{
			fwrite("\xFF\xFE", 1, 2, f);
			fclose(f);
		}
	}
	Ini ini(fileName);
	bool ok = ini.WriteInt(L"frmMain", L"AppWidth", mainWindow.width);
	if (!ok)
		return false;
	ini.WriteInt(L"frmMain", L"AppHeight", mainWindow.height);
	ini.WriteInt(L"frmMain", L"AppPositionX", mainWindow.posX);
	ini.WriteInt(L"frmMain", L"AppPositionY", mainWindow.posY);
	ini.WriteBool(L"frmMain", L"Maximized", mainWindow.maximized);
	ini.WriteBool(L"frmMain", L"AlwaysOnTop", mainWindow.alwaysOnTop);
	ini.WriteString(L"frmMain", L"ColumnWidths", mainWindow.columnWidths);

	ini.WriteBool(L"Logging", L"LogToFile", logging.logToFile);
	ini.WriteInt(L"Logging", L"MaxUiLogLines", logging.maxUiLogLines);

	ini.WriteInt(L"Worker", L"Priority", PriorityToLegacy(worker.priority));
	ini.WriteBool(L"Worker", L"PlaySoundWhenDone", worker.playSoundWhenDone);
	ini.WriteInt(L"Worker", L"ThreadCount", worker.threadCount);

	for (size_t i = 0; i < conversion.defResize.size(); i++)
	{
		const auto& r = conversion.defResize[i];
		ini.WriteInt(L"Conversion", std::format(L"DefaultResizeWidth{}", i).c_str(), r.width);
		ini.WriteInt(L"Conversion", std::format(L"DefaultResizePct{}", i).c_str(), (r.resize <= 0 || r.resize >= 1000) ? 0 : r.resize);
	}
	std::wstring ext = Conversion::FormatExtension(conversion.format);
	ini.WriteString(L"Conversion", L"OutputExtension", ext.substr(1));
	ini.WriteInt(L"Conversion", L"Quality", conversion.quality);
	ini.WriteInt(L"Conversion", L"WebpMethod", conversion.webpMethod);
	ini.WriteBool(L"Conversion", L"WebpLossless", conversion.webpLossless);
	ini.WriteBool(L"Conversion", L"KeepOriginalIfSmaller", conversion.keepOriginalIfSmaller);
	ini.WriteString(L"Conversion", L"FilesToSkip", conversion.filesToSkip);
	ini.WriteString(L"Conversion", L"UnpackPassword", conversion.unpackPassword);
	ini.WriteBool(L"Conversion", L"copyFileToOutputOnError", conversion.copyFileToOutputOnError);

	ini.WriteString(L"Tools", L"SevenZipLocation", tools.sevenZipLocation);

	ini.WriteString(L"PdfImport", L"GsLocation", pdfImport.gsLocation);
	ini.WriteString(L"PdfImport", L"GsParams", pdfImport.gsParams);
	ini.WriteString(L"PdfImport", L"GsFilePattern", pdfImport.gsFilePattern);

	ini.WriteInt(L"Directories", L"TmpDirectoryType", directories.tmpDirectoryType);
	ini.WriteString(L"Directories", L"CustomTmpDirectory", directories.customTmpDirectory);
	ini.WriteInt(L"Directories", L"OutDirectoryType", directories.outDirectoryType);
	ini.WriteString(L"Directories", L"CustomOutDirectory", directories.customOutDirectory);
	ini.WriteBool(L"Directories", L"UseSourceDirectoryForOutput", directories.useSourceDirectoryForOutput);
	ini.WriteBool(L"Directories", L"RecreateSourceDirectoryForOutput", directories.recreateSourceDirectoryForOutput);
	ini.WriteBool(L"Directories", L"UseNumericTmpFileDirectory", directories.useNumericTmpFileDirectory);
	return true;
}

}	// namespace cbx
