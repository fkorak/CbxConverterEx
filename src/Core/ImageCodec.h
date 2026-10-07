#pragma once

#include "Settings.h"

#include <filesystem>
#include <string>

namespace cbx
{

struct EncodeOptions
{
	Settings::Conversion::Format format = Settings::Conversion::FormatWebp;
	int quality = 75;
	int webpMethod = 4;
	bool webpLossless = false;
	bool keepOriginalIfSmaller = false;
};

/** Versions of image libraries, for about box */
std::string CodecLibraryVersions();

/** Read image dimensions from file header (no full decoding).
	\return false if file is not a supported image
*/
bool ReadImageInfo(const std::filesystem::path& path, int& width, int& height);

struct ConvertResult
{
	bool ok = false;
	std::filesystem::path outPath;	///< resulting file (may be original if kept)
	int width = 0;
	int height = 0;
	uint64_t size = 0;
	std::string error;
};

/** Convert single image file.
	\param targetWidth, targetHeight requested size, 0 = keep original size
	Source file is deleted if output file name differs from source.
*/
ConvertResult ConvertImage(const std::filesystem::path& src, int targetWidth, int targetHeight, const EncodeOptions& opts);

}	// namespace cbx
