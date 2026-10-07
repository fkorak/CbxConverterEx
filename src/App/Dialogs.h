#pragma once

#include <string>
#include <vector>

namespace app
{

HWND GetWindowHandle(winrt::Microsoft::UI::Xaml::Window const& window);

struct FileFilter
{
	const wchar_t* name;
	const wchar_t* spec;
};

/** Native file open dialog; returns empty vector if cancelled */
std::vector<std::wstring> PickFiles(HWND owner, const std::vector<FileFilter>& filters, bool multiSelect, const std::wstring& initialFile = L"");
/** Native folder picker; returns empty string if cancelled */
std::wstring PickFolder(HWND owner, const std::wstring& initialDir = L"");

/** ContentDialog with default (modern) style */
winrt::Microsoft::UI::Xaml::Controls::ContentDialog MakeDialog(winrt::Microsoft::UI::Xaml::XamlRoot root, winrt::hstring title);

/** Size dialog content to its preferred size, limited to the space available in the window
	(content should scroll internally). Follows window resizing while the dialog is open.
*/
void FitContentToWindow(winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& dialog,
	winrt::Microsoft::UI::Xaml::FrameworkElement const& content, double preferredWidth, double preferredHeight);

/** Simple message box (ContentDialog). Returns true if primary button was pressed. */
winrt::Windows::Foundation::IAsyncOperation<bool> ShowMessageAsync(winrt::Microsoft::UI::Xaml::XamlRoot root,
	winrt::hstring title, winrt::hstring text, winrt::hstring primaryButton = L"", winrt::hstring closeButton = L"OK");

/** Open file or folder with associated application */
void ShellOpen(const std::wstring& path);

}	// namespace app
