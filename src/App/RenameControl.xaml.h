#pragma once

#include "RenameControl.g.h"
#include "SourceFile.h"

#include <optional>

namespace winrt::CbxConverter::implementation
{
	struct RenameControl : RenameControlT<RenameControl>
	{
		RenameControl() = default;

		void SetFile(std::shared_ptr<cbx::SourceFile> file, HWND owner);

		void OnOptionsChanged(IInspectable const&, IInspectable const&);
		void OnOffsetChanged(Microsoft::UI::Xaml::Controls::NumberBox const&, Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const&);
		void OnRename(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnOpenFile(IInspectable const&, IInspectable const&);
		void OnDeleteFiles(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

		/** Format number using printf-like pattern with exactly one integer conversion (%d, %04d, ...) */
		static std::optional<std::wstring> FormatPattern(const std::wstring& pattern, int number);

	private:
		/** New full path for file; empty if name cannot be created */
		std::filesystem::path GetNewName(const std::filesystem::path& name, int index);
		void Rebuild();
		void UpdateNewNames();
		void ShowError(const std::wstring& text);

		std::shared_ptr<cbx::SourceFile> file;
		Windows::Foundation::Collections::IObservableVector<CbxConverter::RenameItem> items;
		HWND owner = nullptr;
	};
}

namespace winrt::CbxConverter::factory_implementation
{
	struct RenameControl : RenameControlT<RenameControl, implementation::RenameControl>
	{
	};
}
