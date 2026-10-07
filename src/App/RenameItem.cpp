#include "pch.h"
#include "RenameItem.h"
#if __has_include("RenameItem.g.cpp")
#include "RenameItem.g.cpp"
#endif

namespace winrt::CbxConverter::implementation
{
	RenameItem::RenameItem(hstring oldName, uint64_t sz, int w, int h)
		: oldName(std::move(oldName)),
		size(std::to_wstring(sz)),
		width(w > 0 ? hstring(std::to_wstring(w)) : hstring()),
		height(h > 0 ? hstring(std::to_wstring(h)) : hstring())
	{
	}

	void RenameItem::SetOldName(hstring const& value)
	{
		if (oldName == value)
			return;
		oldName = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L"OldName"));
	}

	void RenameItem::SetNewName(hstring const& value)
	{
		if (newName == value)
			return;
		newName = value;
		propertyChanged(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs(L"NewName"));
	}
}
