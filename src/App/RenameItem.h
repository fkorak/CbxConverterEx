#pragma once

#include "RenameItem.g.h"

namespace winrt::CbxConverter::implementation
{
	struct RenameItem : RenameItemT<RenameItem>
	{
		RenameItem(hstring oldName, uint64_t size, int width, int height);

		hstring OldName() const { return oldName; }
		hstring NewName() const { return newName; }
		hstring Size() const { return size; }
		hstring ImageWidth() const { return width; }
		hstring ImageHeight() const { return height; }

		void SetOldName(hstring const& value);
		void SetNewName(hstring const& value);

		winrt::event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
		{
			return propertyChanged.add(handler);
		}
		void PropertyChanged(winrt::event_token const& token) noexcept
		{
			propertyChanged.remove(token);
		}

	private:
		hstring oldName, newName, size, width, height;
		winrt::event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged;
	};
}
