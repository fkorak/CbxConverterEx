#pragma once

#include "SourceFileItem.g.h"
#include "SourceFile.h"

namespace winrt::CbxConverter::implementation
{
	struct SourceFileItem : SourceFileItemT<SourceFileItem>
	{
		explicit SourceFileItem(std::shared_ptr<cbx::SourceFile> file);

		hstring Name() const { return name; }
		hstring FullPath() const { return fullPath; }
		hstring Size() const { return size; }
		hstring Images() const { return images; }
		hstring SizePerImage() const { return sizePerImage; }
		hstring ImageWidth() const { return imageWidth; }
		hstring Resize() const { return resize; }
		hstring Status() const { return status; }
		hstring OutSize() const { return outSize; }
		hstring Ratio() const { return ratio; }
		double Progress() const { return progress; }
		bool ProgressVisible() const { return progressVisible; }
		bool IsError() const { return isError; }

		winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
		{
			return propertyChanged.add(handler);
		}
		void PropertyChanged(winrt::event_token const& token) noexcept
		{
			propertyChanged.remove(token);
		}

		/** Re-read state of source file, raise PropertyChanged for modified values */
		void Refresh();
		const std::shared_ptr<cbx::SourceFile>& File() const { return file; }

	private:
		template <class T>
		void Set(T& field, const T& value, const wchar_t* propertyName);

		std::shared_ptr<cbx::SourceFile> file;
		hstring name, fullPath, size, images, sizePerImage, imageWidth, resize, status, outSize, ratio;
		double progress = 0;
		bool progressVisible = false;
		bool isError = false;
		winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged;
	};

	/** Access native object behind projected item */
	inline SourceFileItem* Native(winrt::CbxConverter::SourceFileItem const& item)
	{
		return winrt::get_self<SourceFileItem>(item);
	}
}
