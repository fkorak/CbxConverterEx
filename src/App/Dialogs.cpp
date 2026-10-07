#include "pch.h"
#include "Dialogs.h"

#include <wil/com.h>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace app
{

HWND GetWindowHandle(Window const& window)
{
	HWND hwnd = nullptr;
	window.as<::IWindowNative>()->get_WindowHandle(&hwnd);
	return hwnd;
}

namespace
{

void SetInitialLocation(IFileDialog* dlg, const std::wstring& path, bool isFile)
{
	if (path.empty())
		return;
	std::filesystem::path p(path);
	std::filesystem::path dir = isFile ? p.parent_path() : p;
	wil::com_ptr<IShellItem> folder;
	if (!dir.empty() && SUCCEEDED(SHCreateItemFromParsingName(dir.c_str(), nullptr, IID_PPV_ARGS(&folder))))
		dlg->SetFolder(folder.get());
	if (isFile)
		dlg->SetFileName(p.filename().c_str());
}

}	// namespace

std::vector<std::wstring> PickFiles(HWND owner, const std::vector<FileFilter>& filters, bool multiSelect, const std::wstring& initialFile)
{
	std::vector<std::wstring> result;
	auto dlg = wil::CoCreateInstanceNoThrow<IFileOpenDialog>(CLSID_FileOpenDialog);
	if (!dlg)
		return result;
	std::vector<COMDLG_FILTERSPEC> specs;
	for (const auto& f : filters)
		specs.push_back({ f.name, f.spec });
	if (!specs.empty())
		dlg->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
	FILEOPENDIALOGOPTIONS opts;
	dlg->GetOptions(&opts);
	opts |= FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM;
	if (multiSelect)
		opts |= FOS_ALLOWMULTISELECT;
	dlg->SetOptions(opts);
	SetInitialLocation(dlg.get(), initialFile, true);
	if (FAILED(dlg->Show(owner)))
		return result;
	wil::com_ptr<IShellItemArray> items;
	if (FAILED(dlg->GetResults(&items)))
		return result;
	DWORD count = 0;
	items->GetCount(&count);
	for (DWORD i = 0; i < count; i++)
	{
		wil::com_ptr<IShellItem> item;
		if (FAILED(items->GetItemAt(i, &item)))
			continue;
		wil::unique_cotaskmem_string name;
		if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)))
			result.emplace_back(name.get());
	}
	return result;
}

std::wstring PickFolder(HWND owner, const std::wstring& initialDir)
{
	auto dlg = wil::CoCreateInstanceNoThrow<IFileOpenDialog>(CLSID_FileOpenDialog);
	if (!dlg)
		return {};
	FILEOPENDIALOGOPTIONS opts;
	dlg->GetOptions(&opts);
	dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
	SetInitialLocation(dlg.get(), initialDir, false);
	if (FAILED(dlg->Show(owner)))
		return {};
	wil::com_ptr<IShellItem> item;
	if (FAILED(dlg->GetResult(&item)))
		return {};
	wil::unique_cotaskmem_string name;
	if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)))
		return {};
	return name.get();
}

ContentDialog MakeDialog(XamlRoot root, hstring title)
{
	ContentDialog dialog;
	dialog.XamlRoot(root);
	dialog.Title(box_value(title));
	if (auto style = Application::Current().Resources().TryLookup(box_value(L"DefaultContentDialogStyle")))
		dialog.Style(style.as<Style>());
	return dialog;
}

void FitContentToWindow(ContentDialog const& dialog, FrameworkElement const& content, double preferredWidth, double preferredHeight)
{
	// space taken by the dialog itself: padding, title, button row
	constexpr double horizontalReserve = 48 + 32;
	constexpr double verticalReserve = 48 + 52 + 80 + 32;
	dialog.Resources().Insert(box_value(L"ContentDialogMaxWidth"), box_value(preferredWidth + horizontalReserve));
	dialog.Resources().Insert(box_value(L"ContentDialogMaxHeight"), box_value(preferredHeight + verticalReserve));

	auto update = [dialog, content, preferredWidth, preferredHeight]() {
		auto root = dialog.XamlRoot();
		if (!root)
			return;
		auto size = root.Size();
		content.Width(std::clamp(size.Width - horizontalReserve, 200.0, preferredWidth));
		content.Height(std::clamp(size.Height - verticalReserve, 150.0, preferredHeight));
	};
	update();
	auto token = dialog.XamlRoot().Changed([update](auto&&, auto&&) { update(); });
	dialog.Closed([token](ContentDialog const& d, auto&&) {
		if (auto root = d.XamlRoot())
			root.Changed(token);
	});
}

Windows::Foundation::IAsyncOperation<bool> ShowMessageAsync(XamlRoot root, hstring title, hstring text, hstring primaryButton, hstring closeButton)
{
	ContentDialog dialog = MakeDialog(root, title);
	TextBlock tb;
	tb.Text(text);
	tb.TextWrapping(TextWrapping::Wrap);
	tb.IsTextSelectionEnabled(true);
	dialog.Content(tb);
	if (!primaryButton.empty())
	{
		dialog.PrimaryButtonText(primaryButton);
		dialog.DefaultButton(ContentDialogButton::Close);
	}
	dialog.CloseButtonText(closeButton);
	auto result = co_await dialog.ShowAsync();
	co_return result == ContentDialogResult::Primary;
}

void ShellOpen(const std::wstring& path)
{
	ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

}	// namespace app
