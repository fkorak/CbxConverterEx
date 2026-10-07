#pragma once

#include <filesystem>
#include <string>
#include <array>

namespace cbx
{

/** Application settings, stored in an INI file next to the executable.
	Key names are compatible with the original (C++Builder) CbxConverter.
*/
class Settings
{
public:
	bool Read(const std::filesystem::path& fileName);
	bool Write(const std::filesystem::path& fileName) const;

	struct MainWindow
	{
		int posX = 30, posY = 30;
		int width = 1100, height = 700;
		bool maximized = false;
		bool alwaysOnTop = false;
		std::wstring columnWidths;	///< file list column widths (DIP), comma separated; empty = defaults
	} mainWindow;

	struct Logging
	{
		bool logToFile = false;
		unsigned int maxUiLogLines = 1000;
	} logging;

	struct Worker
	{
		enum Priority
		{
			PriorityIdle = 0,
			PriorityBelowNormal,
			PriorityNormal,
			PriorityAboveNormal
		} priority = PriorityBelowNormal;
		bool playSoundWhenDone = true;
		static constexpr int THREAD_COUNT_MIN = 1;
		static constexpr int THREAD_COUNT_MAX = 256;
		int threadCount = 0;			///< number of images converted in parallel
		static int DefaultThreadCount();
	} worker;

	struct Conversion
	{
		std::wstring unpackPassword;
		struct DefResize
		{
			int width = 0;
			int resize = 100;
		};
		std::array<DefResize, 10> defResize;

		enum Format
		{
			FormatWebp = 0,
			FormatJpeg,
			FormatPng
		} format = FormatWebp;
		int quality = 75;				///< lossy quality, 1..100 (WebP, JPEG)
		int webpMethod = 4;				///< 0 = fastest .. 6 = slowest/smallest
		bool webpLossless = false;
		bool keepOriginalIfSmaller = false;	///< keep source image if converted one is not smaller
		std::wstring filesToSkip;		///< multiple DOS-like file masks, separated with semicolon
		bool copyFileToOutputOnError = false;

		static const wchar_t* FormatExtension(Format f);
		static const wchar_t* FormatName(Format f);
	} conversion;

	struct Tools
	{
		std::wstring sevenZipLocation;	///< optional, 7z.exe used as fallback for unpacking (e.g. encrypted archives)
	} tools;

	struct PdfImport
	{
		std::wstring gsLocation;
		std::wstring gsParams = L"-sDEVICE=png16m -r300x300";
		std::wstring gsFilePattern = L"%03d.png";
	} pdfImport;

	struct Directories
	{
		enum DirectoryType
		{
			DirectoryTypeDefault = 0,
			DirectoryTypeCustom,

			DirectoryTypeLimiter
		};
		static const wchar_t* DirectoryTypeDescription(DirectoryType type);
		DirectoryType tmpDirectoryType = DirectoryTypeDefault;
		std::wstring customTmpDirectory;
		DirectoryType outDirectoryType = DirectoryTypeDefault;
		std::wstring customOutDirectory;

		bool useSourceDirectoryForOutput = false;
		bool recreateSourceDirectoryForOutput = false;
		bool useNumericTmpFileDirectory = false;	///< use number (e.g. "0000") instead of full file name for temporary directory to shorten path
	} directories;

	int GetDefaultResize(int width) const;
};

}	// namespace cbx
