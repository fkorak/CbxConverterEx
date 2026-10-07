#include "pch.h"
#include "SourceFileItem.h"
#if __has_include("SourceFileItem.g.cpp")
#include "SourceFileItem.g.cpp"
#endif

namespace winrt::CbxConverter::implementation
{
	SourceFileItem::SourceFileItem(std::shared_ptr<cbx::SourceFile> f, CbxConverter::ColumnLayout l) : file(std::move(f)), layout(std::move(l))
	{
		name = file->DisplayName();
		fullPath = file->name.wstring();
		Refresh();
	}

	template <class T>
	void SourceFileItem::Set(T& field, const T& value, const wchar_t* propertyName)
	{
		if (field == value)
			return;
		field = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(propertyName));
	}

	void SourceFileItem::Refresh()
	{
		const cbx::SourceFile& f = *file;
		std::wstring sImages = L"?", sPerImage = L"?", sWidth = L"not checked", sResize, sOut = L"---", sRatio = L"---";
		{
			std::lock_guard lock(f.mutex);
			if (f.widthValid)
			{
				sImages = std::to_wstring(f.imgCount);
				sPerImage = f.imgCount ? std::to_wstring(f.size / f.imgCount / 1024) : L"-";
				sWidth = f.asWidth;
			}
			if (f.resizeCfg.mode == cbx::ResizeCfg::ModeRegular)
				sResize = std::format(L"{}%", f.resizeCfg.resizePct);
			else
				sResize = std::format(L"if > {} → {}", f.resizeCfg.resizeThreshold, f.resizeCfg.resizeTarget);
			if (f.outSizeValid)
			{
				sOut = std::to_wstring(f.outSize / 1024);
				if (f.size)
					sRatio = std::format(L"{:.1f}%", static_cast<double>(f.outSize) * 100.0 / static_cast<double>(f.size));
			}
		}
		Set(size, hstring(std::to_wstring(f.size / 1024)), L"Size");
		Set(images, hstring(sImages), L"Images");
		Set(sizePerImage, hstring(sPerImage), L"SizePerImage");
		Set(imageWidth, hstring(sWidth), L"ImageWidth");
		Set(resize, hstring(sResize), L"Resize");
		Set(status, hstring(f.StateName()), L"Status");
		Set(outSize, hstring(sOut), L"OutSize");
		Set(ratio, hstring(sRatio), L"Ratio");
		int p = f.stateProgress;
		Set(progressVisible, p >= 0, L"ProgressVisible");
		Set(progress, static_cast<double>(std::max(p, 0)), L"Progress");
		Set(isError, f.state == cbx::SourceFile::SOURCE_ERROR, L"IsError");
	}
}
