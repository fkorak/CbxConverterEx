#include "ImageCodec.h"
#include "StringUtils.h"

#include <turbojpeg.h>
#include <jconfig.h>
#include <png.h>
#include <webp/decode.h>
#include <webp/encode.h>

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_PSD
#define STBI_ONLY_PNM
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 4505)	// unreferenced function
#include <stb_image.h>
#pragma warning(pop)
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include <windows.h>
#include <cstring>
#include <memory>
#include <vector>

namespace cbx
{

namespace
{

using Bytes = std::vector<uint8_t>;

enum class FileType
{
	Unknown,
	Jpeg,
	Png,
	Webp,
	Other	///< anything stb_image may recognize
};

/** Decoded 8-bit image: 1 (gray), 3 (RGB) or 4 (RGBA) channels, tightly packed */
struct Bitmap
{
	int width = 0;
	int height = 0;
	int channels = 0;
	Bytes pixels;
	size_t Stride() const
	{
		return static_cast<size_t>(width) * channels;
	}
};

bool ReadFileBytes(const std::filesystem::path& path, Bytes& data, size_t maxBytes = SIZE_MAX)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	LARGE_INTEGER size;
	if (!GetFileSizeEx(h, &size) || size.QuadPart > 0x7FFFFFFF)
	{
		CloseHandle(h);
		return false;
	}
	size_t toRead = static_cast<size_t>(size.QuadPart);
	if (toRead > maxBytes)
		toRead = maxBytes;
	data.resize(toRead);
	DWORD read = 0;
	BOOL ok = toRead == 0 || ReadFile(h, data.data(), static_cast<DWORD>(toRead), &read, nullptr);
	CloseHandle(h);
	if (!ok || read != toRead)
		return false;
	return true;
}

bool WriteFileBytes(const std::filesystem::path& path, const uint8_t* data, size_t size)
{
	HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (h == INVALID_HANDLE_VALUE)
		return false;
	DWORD written = 0;
	BOOL ok = WriteFile(h, data, static_cast<DWORD>(size), &written, nullptr);
	CloseHandle(h);
	if (!ok || written != size)
	{
		DeleteFileW(path.c_str());
		return false;
	}
	return true;
}

FileType DetectType(const uint8_t* d, size_t n)
{
	if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF)
		return FileType::Jpeg;
	if (n >= 8 && memcmp(d, "\x89PNG\r\n\x1a\n", 8) == 0)
		return FileType::Png;
	if (n >= 12 && memcmp(d, "RIFF", 4) == 0 && memcmp(d + 8, "WEBP", 4) == 0)
		return FileType::Webp;
	return FileType::Other;
}

uint32_t ReadBE32(const uint8_t* p)
{
	return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

struct TjDeleter
{
	void operator()(void* h) const
	{
		tj3Destroy(h);
	}
};
using TjHandle = std::unique_ptr<void, TjDeleter>;

bool JpegInfo(const Bytes& data, int& w, int& h)
{
	TjHandle tj(tj3Init(TJINIT_DECOMPRESS));
	if (!tj || tj3DecompressHeader(tj.get(), data.data(), data.size()) != 0)
		return false;
	w = tj3Get(tj.get(), TJPARAM_JPEGWIDTH);
	h = tj3Get(tj.get(), TJPARAM_JPEGHEIGHT);
	return w > 0 && h > 0;
}

/** \param targetWidth/Height used to select DCT scaling (faster decoding of images that are downscaled anyway) */
bool DecodeJpeg(const Bytes& data, int targetWidth, int targetHeight, Bitmap& bmp, std::string& error)
{
	TjHandle tj(tj3Init(TJINIT_DECOMPRESS));
	if (!tj)
	{
		error = "tj3Init failed";
		return false;
	}
	if (tj3DecompressHeader(tj.get(), data.data(), data.size()) != 0)
	{
		error = tj3GetErrorStr(tj.get());
		return false;
	}
	int w = tj3Get(tj.get(), TJPARAM_JPEGWIDTH);
	int h = tj3Get(tj.get(), TJPARAM_JPEGHEIGHT);
	int colorspace = tj3Get(tj.get(), TJPARAM_COLORSPACE);

	if (targetWidth > 0 && targetHeight > 0)
	{
		// pick strongest DCT scaling that still yields at least requested size
		static const tjscalingfactor factors[] = { {1, 8}, {1, 4}, {1, 2} };
		for (const auto& sf : factors)
		{
			if (TJSCALED(w, sf) >= targetWidth && TJSCALED(h, sf) >= targetHeight)
			{
				if (tj3SetScalingFactor(tj.get(), sf) == 0)
				{
					w = TJSCALED(w, sf);
					h = TJSCALED(h, sf);
				}
				break;
			}
		}
	}

	int pf;
	switch (colorspace)
	{
	case TJCS_GRAY:
		pf = TJPF_GRAY;
		bmp.channels = 1;
		break;
	case TJCS_CMYK:
	case TJCS_YCCK:
		pf = TJPF_CMYK;
		bmp.channels = 4;
		break;
	default:
		pf = TJPF_RGB;
		bmp.channels = 3;
		break;
	}
	bmp.width = w;
	bmp.height = h;
	bmp.pixels.resize(bmp.Stride() * h);
	if (tj3Decompress8(tj.get(), data.data(), data.size(), bmp.pixels.data(), static_cast<int>(bmp.Stride()), pf) != 0)
	{
		if (tj3GetErrorCode(tj.get()) != TJERR_WARNING)
		{
			error = tj3GetErrorStr(tj.get());
			return false;
		}
	}

	if (pf == TJPF_CMYK)
	{
		// Adobe CMYK JPEGs are stored inverted; convert to RGB in place
		Bytes rgb(static_cast<size_t>(w) * h * 3);
		const uint8_t* s = bmp.pixels.data();
		uint8_t* d = rgb.data();
		for (size_t i = 0, n = static_cast<size_t>(w) * h; i < n; i++, s += 4, d += 3)
		{
			unsigned k = s[3];
			d[0] = static_cast<uint8_t>(s[0] * k / 255);
			d[1] = static_cast<uint8_t>(s[1] * k / 255);
			d[2] = static_cast<uint8_t>(s[2] * k / 255);
		}
		bmp.pixels.swap(rgb);
		bmp.channels = 3;
	}
	return true;
}

bool DecodePng(const Bytes& data, Bitmap& bmp, std::string& error)
{
	png_image img{};
	img.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_memory(&img, data.data(), data.size()))
	{
		error = img.message;
		return false;
	}
	if (img.format & PNG_FORMAT_FLAG_ALPHA)
	{
		img.format = PNG_FORMAT_RGBA;
		bmp.channels = 4;
	}
	else if (img.format & PNG_FORMAT_FLAG_COLOR)
	{
		img.format = PNG_FORMAT_RGB;
		bmp.channels = 3;
	}
	else
	{
		img.format = PNG_FORMAT_GRAY;
		bmp.channels = 1;
	}
	bmp.width = static_cast<int>(img.width);
	bmp.height = static_cast<int>(img.height);
	bmp.pixels.resize(PNG_IMAGE_SIZE(img));
	if (!png_image_finish_read(&img, nullptr, bmp.pixels.data(), 0, nullptr))
	{
		error = img.message;
		png_image_free(&img);
		return false;
	}
	return true;
}

bool DecodeWebp(const Bytes& data, Bitmap& bmp, std::string& error)
{
	WebPBitstreamFeatures features;
	if (WebPGetFeatures(data.data(), data.size(), &features) != VP8_STATUS_OK)
	{
		error = "invalid WebP header";
		return false;
	}
	if (features.has_animation)
	{
		error = "animated WebP is not supported";
		return false;
	}
	bmp.width = features.width;
	bmp.height = features.height;
	bmp.channels = features.has_alpha ? 4 : 3;
	bmp.pixels.resize(bmp.Stride() * bmp.height);
	uint8_t* res;
	if (features.has_alpha)
		res = WebPDecodeRGBAInto(data.data(), data.size(), bmp.pixels.data(), bmp.pixels.size(), static_cast<int>(bmp.Stride()));
	else
		res = WebPDecodeRGBInto(data.data(), data.size(), bmp.pixels.data(), bmp.pixels.size(), static_cast<int>(bmp.Stride()));
	if (!res)
	{
		error = "WebP decoding failed";
		return false;
	}
	return true;
}

bool DecodeOther(const Bytes& data, Bitmap& bmp, std::string& error)
{
	int w, h, n;
	stbi_uc* p = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &w, &h, &n, 0);
	if (!p)
	{
		error = std::string("unsupported image: ") + stbi_failure_reason();
		return false;
	}
	bmp.width = w;
	bmp.height = h;
	size_t count = static_cast<size_t>(w) * h;
	if (n == 2)
	{
		// gray + alpha -> RGBA
		bmp.channels = 4;
		bmp.pixels.resize(count * 4);
		for (size_t i = 0; i < count; i++)
		{
			bmp.pixels[i * 4 + 0] = bmp.pixels[i * 4 + 1] = bmp.pixels[i * 4 + 2] = p[i * 2];
			bmp.pixels[i * 4 + 3] = p[i * 2 + 1];
		}
	}
	else
	{
		bmp.channels = n;
		bmp.pixels.assign(p, p + count * n);
	}
	stbi_image_free(p);
	return true;
}

/** Drop alpha channel if image is fully opaque (smaller and faster to encode) */
void StripOpaqueAlpha(Bitmap& bmp)
{
	if (bmp.channels != 4)
		return;
	size_t count = static_cast<size_t>(bmp.width) * bmp.height;
	for (size_t i = 0; i < count; i++)
	{
		if (bmp.pixels[i * 4 + 3] != 255)
			return;
	}
	for (size_t i = 0; i < count; i++)
	{
		bmp.pixels[i * 3 + 0] = bmp.pixels[i * 4 + 0];
		bmp.pixels[i * 3 + 1] = bmp.pixels[i * 4 + 1];
		bmp.pixels[i * 3 + 2] = bmp.pixels[i * 4 + 2];
	}
	bmp.pixels.resize(count * 3);
	bmp.channels = 3;
}

bool Resize(Bitmap& bmp, int w, int h, std::string& error)
{
	stbir_pixel_layout layout = bmp.channels == 1 ? STBIR_1CHANNEL : (bmp.channels == 3 ? STBIR_RGB : STBIR_RGBA);
	Bitmap out;
	out.width = w;
	out.height = h;
	out.channels = bmp.channels;
	out.pixels.resize(out.Stride() * h);
	// Catmull-Rom: sharp result for line art, little ringing
	if (!stbir_resize(bmp.pixels.data(), bmp.width, bmp.height, static_cast<int>(bmp.Stride()),
		out.pixels.data(), w, h, static_cast<int>(out.Stride()),
		layout, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_CATMULLROM))
	{
		error = "resizing failed";
		return false;
	}
	bmp = std::move(out);
	return true;
}

bool EncodeWebp(const Bitmap& bmp, const EncodeOptions& opts, Bytes& out, std::string& error)
{
	if (bmp.width > WEBP_MAX_DIMENSION || bmp.height > WEBP_MAX_DIMENSION)
	{
		error = "image too large for WebP (max 16383 px)";
		return false;
	}
	WebPConfig config;
	if (!WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, static_cast<float>(opts.quality)))
	{
		error = "WebPConfigPreset failed";
		return false;
	}
	config.method = opts.webpMethod;
	config.lossless = opts.webpLossless ? 1 : 0;
	config.thread_level = 0;	// parallelism is done per image
	if (!WebPValidateConfig(&config))
	{
		error = "invalid WebP configuration";
		return false;
	}

	WebPPicture pic;
	if (!WebPPictureInit(&pic))
	{
		error = "WebPPictureInit failed";
		return false;
	}
	pic.width = bmp.width;
	pic.height = bmp.height;
	int ok;
	const int stride = static_cast<int>(bmp.Stride());
	if (bmp.channels == 1 && !config.lossless)
	{
		// grayscale: fill luma plane directly, neutral chroma
		pic.use_argb = 0;
		pic.colorspace = WEBP_YUV420;
		ok = WebPPictureAlloc(&pic);
		if (ok)
		{
			for (int y = 0; y < bmp.height; y++)
				memcpy(pic.y + static_cast<size_t>(y) * pic.y_stride, bmp.pixels.data() + static_cast<size_t>(y) * stride, bmp.width);
			int uvHeight = (bmp.height + 1) / 2;
			int uvWidth = (bmp.width + 1) / 2;
			for (int y = 0; y < uvHeight; y++)
			{
				memset(pic.u + static_cast<size_t>(y) * pic.uv_stride, 128, uvWidth);
				memset(pic.v + static_cast<size_t>(y) * pic.uv_stride, 128, uvWidth);
			}
		}
	}
	else if (bmp.channels == 1)
	{
		Bytes rgb(static_cast<size_t>(bmp.width) * bmp.height * 3);
		for (size_t i = 0; i < bmp.pixels.size(); i++)
			rgb[i * 3] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = bmp.pixels[i];
		pic.use_argb = 1;
		ok = WebPPictureImportRGB(&pic, rgb.data(), bmp.width * 3);
	}
	else
	{
		pic.use_argb = config.lossless ? 1 : 0;
		if (bmp.channels == 4)
			ok = WebPPictureImportRGBA(&pic, bmp.pixels.data(), stride);
		else
			ok = WebPPictureImportRGB(&pic, bmp.pixels.data(), stride);
	}
	if (!ok)
	{
		WebPPictureFree(&pic);
		error = "WebP picture import failed (out of memory?)";
		return false;
	}

	WebPMemoryWriter writer;
	WebPMemoryWriterInit(&writer);
	pic.writer = WebPMemoryWrite;
	pic.custom_ptr = &writer;
	ok = WebPEncode(&config, &pic);
	if (ok)
		out.assign(writer.mem, writer.mem + writer.size);
	else
		error = "WebP encoding failed, error code " + std::to_string(pic.error_code);
	WebPMemoryWriterClear(&writer);
	WebPPictureFree(&pic);
	return ok != 0;
}

bool EncodeJpeg(const Bitmap& bmp, const EncodeOptions& opts, Bytes& out, std::string& error)
{
	TjHandle tj(tj3Init(TJINIT_COMPRESS));
	if (!tj)
	{
		error = "tj3Init failed";
		return false;
	}
	const Bitmap* src = &bmp;
	Bitmap flattened;
	if (bmp.channels == 4)
	{
		// JPEG has no alpha - composite over white
		flattened.width = bmp.width;
		flattened.height = bmp.height;
		flattened.channels = 3;
		size_t count = static_cast<size_t>(bmp.width) * bmp.height;
		flattened.pixels.resize(count * 3);
		for (size_t i = 0; i < count; i++)
		{
			unsigned a = bmp.pixels[i * 4 + 3];
			for (int c = 0; c < 3; c++)
				flattened.pixels[i * 3 + c] = static_cast<uint8_t>((bmp.pixels[i * 4 + c] * a + 255 * (255 - a)) / 255);
		}
		src = &flattened;
	}
	tj3Set(tj.get(), TJPARAM_QUALITY, opts.quality);
	tj3Set(tj.get(), TJPARAM_SUBSAMP, src->channels == 1 ? TJSAMP_GRAY : TJSAMP_420);
	tj3Set(tj.get(), TJPARAM_OPTIMIZE, 1);
	unsigned char* buf = nullptr;
	size_t size = 0;
	int rc = tj3Compress8(tj.get(), src->pixels.data(), src->width, static_cast<int>(src->Stride()), src->height,
		src->channels == 1 ? TJPF_GRAY : TJPF_RGB, &buf, &size);
	if (rc != 0)
	{
		error = tj3GetErrorStr(tj.get());
		tj3Free(buf);
		return false;
	}
	out.assign(buf, buf + size);
	tj3Free(buf);
	return true;
}

bool EncodePng(const Bitmap& bmp, Bytes& out, std::string& error)
{
	png_image img{};
	img.version = PNG_IMAGE_VERSION;
	img.width = bmp.width;
	img.height = bmp.height;
	img.format = bmp.channels == 1 ? PNG_FORMAT_GRAY : (bmp.channels == 3 ? PNG_FORMAT_RGB : PNG_FORMAT_RGBA);
	png_alloc_size_t size = 0;
	if (!png_image_write_get_memory_size(img, size, 0, bmp.pixels.data(), 0, nullptr))
	{
		error = img.message;
		return false;
	}
	out.resize(size);
	if (!png_image_write_to_memory(&img, out.data(), &size, 0, bmp.pixels.data(), 0, nullptr))
	{
		error = img.message;
		return false;
	}
	out.resize(size);
	return true;
}

}	// namespace

std::string CodecLibraryVersions()
{
	auto webpVersion = [](int v) {
		return std::to_string((v >> 16) & 0xFF) + "." + std::to_string((v >> 8) & 0xFF) + "." + std::to_string(v & 0xFF);
	};
	return "libwebp " + webpVersion(WebPGetEncoderVersion()) +
		", libjpeg-turbo " + std::to_string(LIBJPEG_TURBO_VERSION_NUMBER / 1000000) + "." +
		std::to_string(LIBJPEG_TURBO_VERSION_NUMBER / 1000 % 1000) + "." + std::to_string(LIBJPEG_TURBO_VERSION_NUMBER % 1000) +
		", libpng " PNG_LIBPNG_VER_STRING;
}

bool ReadImageInfo(const std::filesystem::path& path, int& width, int& height)
{
	Bytes data;
	// JPEG headers may follow large EXIF/ICC blocks, read whole file for JPEG
	if (!ReadFileBytes(path, data, 64 * 1024))
		return false;
	switch (DetectType(data.data(), data.size()))
	{
	case FileType::Jpeg:
		if (JpegInfo(data, width, height))
			return true;
		return ReadFileBytes(path, data) && JpegInfo(data, width, height);
	case FileType::Png:
		if (data.size() < 24 || memcmp(data.data() + 12, "IHDR", 4) != 0)
			return false;
		width = static_cast<int>(ReadBE32(data.data() + 16));
		height = static_cast<int>(ReadBE32(data.data() + 20));
		return width > 0 && height > 0;
	case FileType::Webp:
		return WebPGetInfo(data.data(), data.size(), &width, &height) != 0;
	default:
	{
		int n;
		return stbi_info_from_memory(data.data(), static_cast<int>(data.size()), &width, &height, &n) != 0;
	}
	}
}

ConvertResult ConvertImage(const std::filesystem::path& src, int targetWidth, int targetHeight, const EncodeOptions& opts)
{
	ConvertResult res;
	Bytes data;
	if (!ReadFileBytes(src, data))
	{
		res.error = "cannot read file";
		return res;
	}

	Bitmap bmp;
	bool decoded = false;
	switch (DetectType(data.data(), data.size()))
	{
	case FileType::Jpeg:
		decoded = DecodeJpeg(data, targetWidth, targetHeight, bmp, res.error);
		break;
	case FileType::Png:
		decoded = DecodePng(data, bmp, res.error);
		break;
	case FileType::Webp:
		decoded = DecodeWebp(data, bmp, res.error);
		break;
	default:
		decoded = DecodeOther(data, bmp, res.error);
		break;
	}
	if (!decoded)
		return res;

	StripOpaqueAlpha(bmp);

	bool resized = false;
	if (targetWidth > 0 && targetHeight > 0 && (targetWidth != bmp.width || targetHeight != bmp.height))
	{
		if (!Resize(bmp, targetWidth, targetHeight, res.error))
			return res;
		resized = true;
	}

	Bytes encoded;
	bool encodedOk;
	switch (opts.format)
	{
	case Settings::Conversion::FormatJpeg:
		encodedOk = EncodeJpeg(bmp, opts, encoded, res.error);
		break;
	case Settings::Conversion::FormatPng:
		encodedOk = EncodePng(bmp, encoded, res.error);
		break;
	default:
		encodedOk = EncodeWebp(bmp, opts, encoded, res.error);
		break;
	}
	if (!encodedOk)
		return res;

	if (opts.keepOriginalIfSmaller && !resized && encoded.size() >= data.size())
	{
		res.ok = true;
		res.outPath = src;
		res.width = bmp.width;
		res.height = bmp.height;
		res.size = data.size();
		return res;
	}

	std::filesystem::path out = src;
	out.replace_extension(Settings::Conversion::FormatExtension(opts.format));
	bool sameFile = EqualsNoCase(out.native(), src.native());
	if (!sameFile && GetFileAttributesW(out.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		// e.g. 001.jpg and 001.png in same archive - do not overwrite
		out = src;
		out += Settings::Conversion::FormatExtension(opts.format);
	}
	std::filesystem::path tmp = out;
	tmp += L".tmp";
	if (!WriteFileBytes(tmp, encoded.data(), encoded.size()))
	{
		res.error = "cannot write output file";
		return res;
	}
	if (sameFile)
		SetFileAttributesW(src.c_str(), FILE_ATTRIBUTE_NORMAL);
	if (!MoveFileExW(tmp.c_str(), out.c_str(), MOVEFILE_REPLACE_EXISTING))
	{
		DeleteFileW(tmp.c_str());
		res.error = "cannot rename output file";
		return res;
	}
	if (!sameFile)
	{
		SetFileAttributesW(src.c_str(), FILE_ATTRIBUTE_NORMAL);
		DeleteFileW(src.c_str());
	}
	res.ok = true;
	res.outPath = out;
	res.width = bmp.width;
	res.height = bmp.height;
	res.size = encoded.size();
	return res;
}

}	// namespace cbx
