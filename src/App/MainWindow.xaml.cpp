#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "SourceFileItem.h"
#include "SettingsControl.xaml.h"
#include "SettingsWindow.xaml.h"
#include "ResizeControl.xaml.h"
#include "RenameControl.xaml.h"
#include "LogWindow.xaml.h"
#include "AppState.h"
#include "Dialogs.h"
#include "resource.h"

#include "Archive.h"
#include "ImageCodec.h"
#include "Log.h"
#include "StringUtils.h"

namespace fs = std::filesystem;

using namespace winrt;

using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Windowing;
using namespace std::chrono_literals;

namespace winrt::CbxConverter::implementation
{
	namespace
	{
		const wchar_t* const columnTitles[] = {
			L"Source file", L"Size [kB]", L"Images", L"kB/img", L"Img width", L"Resize", L"Status", L"Out [kB]", L"Out/in"
		};

		bool IsSupportedArchive(const fs::path& p)
		{
			static const wchar_t* const extensions[] = {
				L".cbz", L".cbr", L".cb7", L".cbt", L".zip", L".rar", L".7z", L".tar", L".pdf"
			};
			std::wstring ext = p.extension().wstring();
			for (auto e : extensions)
			{
				if (cbx::EqualsNoCase(ext, e))
					return true;
			}
			return false;
		}

		/** Natural ("file2" < "file10"), case-insensitive comparison */
		int CompareNatural(const std::wstring& a, const std::wstring& b)
		{
			int r = CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE | SORT_DIGITSASNUMBERS,
				a.c_str(), static_cast<int>(a.size()), b.c_str(), static_cast<int>(b.size()), nullptr, nullptr, 0);
			return r - CSTR_EQUAL;
		}
	}

	MainWindow::MainWindow()
	{
		// Xaml objects should not call InitializeComponent during construction.
		// See https://github.com/microsoft/cppwinrt/tree/master/nuget#initializecomponent
	}

	void MainWindow::InitializeComponent()
	{
		MainWindowT::InitializeComponent();
		items = single_threaded_observable_vector<CbxConverter::SourceFileItem>();
		FileList().ItemsSource(items);
		InitWindow();
	}

	void MainWindow::InitWindow()
	{
		auto& st = app::AppState::Get();
		st.Init();

		HICON icon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
		if (icon)
			AppWindow().SetIcon(Microsoft::UI::GetIconIdFromIcon(icon));
		AppWindow().TitleBar().PreferredTheme(TitleBarTheme::UseDefaultAppMode);
		ApplyWindowSettings();
		AppWindow().Closing({ this, &MainWindow::OnClosing });

		refreshTimer = DispatcherQueue().CreateTimer();
		refreshTimer.Interval(250ms);
		refreshTimer.Tick([this](auto&&, auto&&) { RefreshView(); });
		refreshTimer.Start();

		cbx::LOG("Application started, converting up to {} images in parallel", st.settings.worker.threadCount);

		// files / directories passed on command line ("Open with", dropped on executable)
		int argc = 0;
		if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc))
		{
			std::vector<std::wstring> paths(argv + 1, argv + argc);
			LocalFree(argv);
			AddPaths(paths);
		}
		RefreshView();
	}

	// window placement is stored in DPI-independent units (as the original VCL version did)
	double MainWindow::DpiScale()
	{
		UINT dpi = GetDpiForWindow(app::GetWindowHandle(*this));
		return dpi ? dpi / 96.0 : 1.0;
	}

	void MainWindow::ApplyWindowSettings()
	{
		const auto& s = app::AppState::Get().settings.mainWindow;
		double scale = DpiScale();
		Windows::Graphics::RectInt32 r{
			static_cast<int32_t>(std::lround(s.posX * scale)), static_cast<int32_t>(std::lround(s.posY * scale)),
			static_cast<int32_t>(std::lround(s.width * scale)), static_cast<int32_t>(std::lround(s.height * scale)) };
		auto area = DisplayArea::GetFromRect(r, DisplayAreaFallback::Nearest);
		auto wa = area.WorkArea();
		r.Width = std::min(r.Width, wa.Width);
		r.Height = std::min(r.Height, wa.Height);
		r.X = std::clamp(r.X, wa.X, wa.X + wa.Width - r.Width);
		r.Y = std::clamp(r.Y, wa.Y, wa.Y + wa.Height - r.Height);
		AppWindow().MoveAndResize(r);
		if (auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>())
		{
			presenter.IsAlwaysOnTop(s.alwaysOnTop);
			if (s.maximized)
				presenter.Maximize();
		}
	}

	void MainWindow::SaveWindowPlacement()
	{
		auto& s = app::AppState::Get().settings.mainWindow;
		auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>();
		if (!presenter)
			return;
		s.maximized = presenter.State() == OverlappedPresenterState::Maximized;
		if (presenter.State() == OverlappedPresenterState::Restored)
		{
			auto pos = AppWindow().Position();
			auto size = AppWindow().Size();
			double scale = DpiScale();
			s.posX = static_cast<int>(std::lround(pos.X / scale));
			s.posY = static_cast<int>(std::lround(pos.Y / scale));
			s.width = static_cast<int>(std::lround(size.Width / scale));
			s.height = static_cast<int>(std::lround(size.Height / scale));
		}
	}

	fire_and_forget MainWindow::OnClosing(Microsoft::UI::Windowing::AppWindow const&, AppWindowClosingEventArgs const& args)
	{
		auto& st = app::AppState::Get();
		if (!closeConfirmed && st.engine && !st.engine->IsIdle())
		{
			args.Cancel(true);
			if (dialogOpen)
				co_return;
			auto lifetime = get_strong();
			dialogOpen = true;
			bool exit = co_await app::ShowMessageAsync(Content().XamlRoot(), CBX_APP_NAME,
				L"Conversion is in progress. Abort it and exit?", L"Abort and exit", L"Cancel");
			dialogOpen = false;
			if (exit)
			{
				closeConfirmed = true;
				Close();
			}
			co_return;
		}

		SaveWindowPlacement();
		st.SaveSettings();
		refreshTimer.Stop();
		if (logWindow)
		{
			logWindow.Close();
			logWindow = nullptr;
		}
		if (st.engine)
		{
			st.engine->AbortAll();
			st.engine.reset();	// waits for worker threads
		}
	}

	// ---------------------------------------------------------------- adding files

	fs::path MainWindow::MakeTmpDir(const fs::path& file)
	{
		auto& st = app::AppState::Get();
		std::wstring name = st.settings.directories.useNumericTmpFileDirectory
			? std::format(L"{:04}", numericTmpDirId++)
			: file.filename().wstring();
		auto used = [&](const fs::path& dir) {
			for (auto const& item : items)
			{
				if (cbx::EqualsNoCase(Native(item)->File()->tmpDir.native(), dir.native()))
					return true;
			}
			return false;
		};
		fs::path dir = st.tmpPath / name;
		for (int n = 2; used(dir); n++)
			dir = st.tmpPath / (name + std::format(L" ({})", n));
		return dir;
	}

	void MainWindow::AddSourceFile(const fs::path& path, const fs::path& rootPath)
	{
		auto& st = app::AppState::Get();
		std::error_code ec;
		if (!fs::is_regular_file(path, ec))
			return;
		for (auto const& item : items)
		{
			if (cbx::EqualsNoCase(Native(item)->File()->name.native(), path.native()))
			{
				cbx::LOG("{} is already on the list", cbx::ToUtf8(path.native()));
				return;
			}
		}
		bool isPdf = cbx::EqualsNoCase(path.extension().native(), L".pdf");
		if (isPdf)
		{
			if (st.settings.pdfImport.gsLocation.empty())
			{
				cbx::LOG("Install GhostScript and set gswin64c.exe path in configuration to import {}", cbx::ToUtf8(path.native()));
				return;
			}
			if (!fs::exists(st.settings.pdfImport.gsLocation, ec))
			{
				cbx::LOG("GhostScript location is not valid, see settings");
				return;
			}
		}

		auto file = std::make_shared<cbx::SourceFile>();
		file->name = path;
		file->rootPath = rootPath;
		file->size = fs::file_size(path, ec);
		file->isPdf = isPdf;
		file->tmpDir = MakeTmpDir(path);
		items.Append(make<implementation::SourceFileItem>(file));
		st.engine->AddUnpack(file, st.MakeJobConfig());
	}

	void MainWindow::AddSourceDirectory(const fs::path& dir)
	{
		cbx::LOG("Add source directory: {}", cbx::ToUtf8(dir.native()));
		std::vector<fs::path> files;
		std::error_code ec;
		for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
			!ec && it != fs::recursive_directory_iterator(); it.increment(ec))
		{
			if (it->is_regular_file(ec) && IsSupportedArchive(it->path()))
				files.push_back(it->path());
		}
		std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
			return CompareNatural(a.native(), b.native()) < 0;
		});
		for (const auto& f : files)
			AddSourceFile(f, dir.parent_path());
	}

	void MainWindow::AddPaths(const std::vector<std::wstring>& paths)
	{
		std::error_code ec;
		for (const auto& p : paths)
		{
			if (fs::is_directory(p, ec))
				AddSourceDirectory(p);
			else
				AddSourceFile(p, {});
		}
		RefreshView();
	}

	void MainWindow::OnAddFiles(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = app::PickFiles(app::GetWindowHandle(*this), {
			{ L"Comic book files (*.cbz, *.cbr, *.cb7, *.cbt, *.pdf)", L"*.cbz;*.cbr;*.cb7;*.cbt;*.zip;*.rar;*.7z;*.tar;*.pdf" },
			{ L"All files", L"*.*" } }, true);
		AddPaths(files);
	}

	void MainWindow::OnAddDirectory(IInspectable const&, RoutedEventArgs const&)
	{
		std::wstring dir = app::PickFolder(app::GetWindowHandle(*this));
		if (!dir.empty())
			AddPaths({ dir });
	}

	void MainWindow::OnDragOver(IInspectable const&, DragEventArgs const& e)
	{
		if (e.DataView().Contains(Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
		{
			e.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
			e.DragUIOverride().Caption(L"Add to list");
		}
	}

	fire_and_forget MainWindow::OnDrop(IInspectable const&, DragEventArgs const& e)
	{
		if (!e.DataView().Contains(Windows::ApplicationModel::DataTransfer::StandardDataFormats::StorageItems()))
			co_return;
		auto lifetime = get_strong();
		auto deferral = e.GetDeferral();
		auto storageItems = co_await e.DataView().GetStorageItemsAsync();
		std::vector<std::wstring> paths;
		for (auto const& item : storageItems)
		{
			cbx::LOG("Dropped into application: {}", to_string(item.Path()));
			paths.emplace_back(item.Path());
		}
		deferral.Complete();
		AddPaths(paths);
	}

	// ---------------------------------------------------------------- list

	void MainWindow::RefreshView()
	{
		auto& st = app::AppState::Get();
		if (!st.engine)
			return;
		int done = 0, errors = 0;
		for (auto const& item : items)
		{
			auto impl = Native(item);
			impl->Refresh();
			auto state = impl->File()->state.load();
			if (state == cbx::SourceFile::DONE)
				done++;
			else if (state == cbx::SourceFile::SOURCE_ERROR)
				errors++;
		}
		EmptyHint().Visibility(items.Size() ? Visibility::Collapsed : Visibility::Visible);

		bool idle = st.engine->IsIdle();
		StopButton().IsEnabled(!idle);
		std::wstring status = std::format(L"{} file(s)", items.Size());
		if (done)
			status += std::format(L", {} converted", done);
		if (errors)
			status += std::format(L", {} error(s)", errors);
		if (!idle)
			status += std::format(L"  —  working ({} threads)", st.engine->GetThreadCount());
		StatusText().Text(status);

		if (idle && !wasIdle)
		{
			if (st.settings.worker.playSoundWhenDone)
				MessageBeep(MB_ICONASTERISK);
			if (st.engineReconfigurePending)
				st.engineReconfigurePending = !st.engine->Reconfigure(st.settings.worker.threadCount, st.settings.worker.priority);
		}
		wasIdle = idle;
	}

	std::vector<CbxConverter::SourceFileItem> MainWindow::SelectedItems()
	{
		std::vector<CbxConverter::SourceFileItem> result;
		for (auto const& obj : FileList().SelectedItems())
			result.push_back(obj.as<CbxConverter::SourceFileItem>());
		return result;
	}

	std::vector<std::shared_ptr<cbx::SourceFile>> MainWindow::SelectedFiles()
	{
		std::vector<std::shared_ptr<cbx::SourceFile>> result;
		for (auto const& item : SelectedItems())
			result.push_back(Native(item)->File());
		return result;
	}

	void MainWindow::RemoveItems(const std::vector<CbxConverter::SourceFileItem>& toRemove)
	{
		for (auto const& item : toRemove)
		{
			uint32_t index;
			if (items.IndexOf(item, index))
				items.RemoveAt(index);
		}
		RefreshView();
	}

	void MainWindow::Sort(int column)
	{
		if (column == sortColumn)
			sortAscending = !sortAscending;
		else
			sortAscending = true;
		sortColumn = column;

		struct Entry
		{
			CbxConverter::SourceFileItem item{ nullptr };
			std::wstring text;
			double value = 0;
		};
		std::vector<Entry> entries;
		for (auto const& item : items)
		{
			Entry e;
			e.item = item;
			const cbx::SourceFile& f = *Native(item)->File();
			std::lock_guard lock(f.mutex);
			switch (column)
			{
			case 0:
				e.text = f.DisplayName();
				break;
			case 1:
				e.value = static_cast<double>(f.size);
				break;
			case 2:
				e.value = f.imgCount;
				break;
			case 3:
				e.value = f.imgCount ? static_cast<double>(f.size) / f.imgCount : 0;
				break;
			case 4:
				e.value = f.mostFrequentWidth;
				break;
			case 7:
				e.value = f.outSizeValid ? static_cast<double>(f.outSize) : 0;
				break;
			case 8:
				e.value = (f.outSizeValid && f.size) ? static_cast<double>(f.outSize) / f.size : 0;
				break;
			}
			entries.push_back(std::move(e));
		}
		bool asc = sortAscending;
		std::stable_sort(entries.begin(), entries.end(), [column, asc](const Entry& a, const Entry& b) {
			int c = column == 0 ? CompareNatural(a.text, b.text) : (a.value < b.value ? -1 : (a.value > b.value ? 1 : 0));
			return asc ? c < 0 : c > 0;
		});
		std::vector<CbxConverter::SourceFileItem> sorted;
		for (auto& e : entries)
			sorted.push_back(e.item);
		items.ReplaceAll(sorted);

		for (auto const& child : HeaderGrid().Children())
		{
			if (auto button = child.try_as<Button>())
			{
				int col = std::stoi(unbox_value<hstring>(button.Tag()).c_str());
				std::wstring title = columnTitles[col];
				if (col == sortColumn)
					title += sortAscending ? L" ▲" : L" ▼";
				button.Content(box_value(title));
			}
		}
	}

	void MainWindow::OnHeaderClick(IInspectable const& sender, RoutedEventArgs const&)
	{
		auto button = sender.as<Button>();
		Sort(std::stoi(unbox_value<hstring>(button.Tag()).c_str()));
	}

	void MainWindow::OnFileListKeyDown(IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e)
	{
		if (e.Key() == Windows::System::VirtualKey::Delete)
		{
			OnRemoveFiles(nullptr, nullptr);
			e.Handled(true);
		}
	}

	void MainWindow::OnFileListRightTapped(IInspectable const&, Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e)
	{
		// select item under cursor if it is not part of current selection
		auto element = e.OriginalSource().try_as<FrameworkElement>();
		if (!element)
			return;
		auto item = element.DataContext().try_as<CbxConverter::SourceFileItem>();
		if (!item)
			return;
		for (auto const& obj : FileList().SelectedItems())
		{
			if (obj == item)
				return;
		}
		FileList().SelectedItem(item);
	}

	void MainWindow::OnFileListDoubleTapped(IInspectable const&, Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const&)
	{
		auto files = SelectedFiles();
		if (files.size() != 1)
			return;
		if (files[0]->state == cbx::SourceFile::DONE)
			OnShowOutputFile(nullptr, nullptr);
		else
			OnOpenItemTmp(nullptr, nullptr);
	}

	void MainWindow::OnFileListMenuOpening(IInspectable const&, IInspectable const&)
	{
		auto files = SelectedFiles();
		bool skip = !files.empty();
		for (const auto& f : files)
		{
			std::lock_guard lock(f->mutex);
			if (!f->doNotPack)
			{
				skip = false;
				break;
			}
		}
		SkipPackingItem().IsChecked(skip);

		// these act on the first selected file and need its folder / output file to exist
		bool hasOutput = false, hasTmp = false;
		if (!files.empty())
		{
			fs::path out;
			{
				std::lock_guard lock(files[0]->mutex);
				out = files[0]->outFile;
			}
			std::error_code ec;
			hasOutput = !out.empty() && fs::exists(out, ec);
			hasTmp = fs::exists(files[0]->tmpDir, ec);
		}
		ShowOutputItem().IsEnabled(hasOutput);
		OpenTmpItem().IsEnabled(hasTmp);
	}

	fire_and_forget MainWindow::OnResize(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = SelectedFiles();
		if (files.empty() || dialogOpen)
			co_return;
		auto lifetime = get_strong();
		cbx::ResizeCfg cfg;
		{
			std::lock_guard lock(files[0]->mutex);
			cfg = files[0]->resizeCfg;
		}
		CbxConverter::ResizeControl control;
		get_self<ResizeControl>(control)->Load(cfg);
		auto dialog = app::MakeDialog(Content().XamlRoot(), files.size() > 1 ? hstring(std::format(L"Resize ({} files)", files.size())) : hstring(L"Resize"));
		dialog.Content(control);
		dialog.PrimaryButtonText(L"Apply");
		dialog.CloseButtonText(L"Cancel");
		dialog.DefaultButton(ContentDialogButton::Primary);
		dialogOpen = true;
		auto result = co_await dialog.ShowAsync();
		dialogOpen = false;
		if (result != ContentDialogResult::Primary)
			co_return;
		cfg = get_self<ResizeControl>(control)->Save();
		for (const auto& f : files)
		{
			std::lock_guard lock(f->mutex);
			f->resizeCfg = cfg;
		}
		RefreshView();
	}

	fire_and_forget MainWindow::OnRename(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = SelectedFiles();
		if (files.empty() || dialogOpen)
			co_return;
		auto lifetime = get_strong();
		auto root = Content().XamlRoot();
		auto file = files[0];
		bool unpacked;
		{
			std::lock_guard lock(file->mutex);
			unpacked = file->unpacked;
		}
		if (!app::AppState::Get().engine->IsIdle())
		{
			co_await app::ShowMessageAsync(root, L"Rename", L"Cannot modify files while converting.");
			co_return;
		}
		if (!unpacked)
		{
			co_await app::ShowMessageAsync(root, L"Rename", L"Archive is not unpacked (anymore) - nothing to rename.");
			co_return;
		}
		CbxConverter::RenameControl control;
		get_self<RenameControl>(control)->SetFile(file, app::GetWindowHandle(*this));
		auto dialog = app::MakeDialog(root, hstring(L"Rename images - " + file->name.filename().wstring()));
		dialog.Content(control);
		app::FitContentToWindow(dialog, control, 900, 600);
		dialog.CloseButtonText(L"Close");
		dialogOpen = true;
		co_await dialog.ShowAsync();
		dialogOpen = false;
		RefreshView();
	}

	void MainWindow::OnSkipPacking(IInspectable const&, RoutedEventArgs const&)
	{
		bool skip = SkipPackingItem().IsChecked();
		for (const auto& f : SelectedFiles())
		{
			std::lock_guard lock(f->mutex);
			f->doNotPack = skip;
		}
	}

	void MainWindow::OnOpenItemTmp(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = SelectedFiles();
		std::error_code ec;
		if (!files.empty() && fs::exists(files[0]->tmpDir, ec))
			app::ShellOpen(files[0]->tmpDir.wstring());
	}

	void MainWindow::OnShowOutputFile(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = SelectedFiles();
		if (files.empty())
			return;
		fs::path out;
		{
			std::lock_guard lock(files[0]->mutex);
			out = files[0]->outFile;
		}
		std::error_code ec;
		if (out.empty() || !fs::exists(out, ec))
			return;
		std::wstring args = L"/select,\"" + out.wstring() + L"\"";
		ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
	}

	void MainWindow::OnSelectAll(IInspectable const&, RoutedEventArgs const&)
	{
		FileList().SelectAll();
	}

	void MainWindow::OnRemoveFiles(IInspectable const&, RoutedEventArgs const&)
	{
		std::vector<CbxConverter::SourceFileItem> toRemove;
		int skipped = 0;
		for (auto const& item : SelectedItems())
		{
			auto file = Native(item)->File();
			if (file->jobCount > 0 || file->busy)
			{
				skipped++;
				continue;
			}
			bool keepTmp;
			{
				std::lock_guard lock(file->mutex);
				keepTmp = file->state == cbx::SourceFile::DONE && file->doNotPack;
			}
			std::error_code ec;
			if (!keepTmp && fs::exists(file->tmpDir, ec))
				fs::remove_all(file->tmpDir, ec);
			toRemove.push_back(item);
		}
		if (skipped)
			cbx::LOG("{} file(s) are being processed and were not removed from list", skipped);
		RemoveItems(toRemove);
	}

	fire_and_forget MainWindow::OnDeleteSourceFiles(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = SelectedFiles();
		auto root = Content().XamlRoot();
		auto lifetime = get_strong();
		if (files.empty() || dialogOpen)
			co_return;
		dialogOpen = true;
		bool confirmed = co_await app::ShowMessageAsync(root, L"Delete source files",
			hstring(std::format(L"Delete {} source file(s)?", files.size())), L"Delete", L"Cancel");
		if (confirmed)
		{
			int deleted = 0;
			for (const auto& f : files)
			{
				auto state = f->state.load();
				if (state == cbx::SourceFile::UNPACKING || state == cbx::SourceFile::RENDERING_PDF)
				{
					cbx::LOG("Cannot delete file that is unpacking right now, skipping: {}", cbx::ToUtf8(f->name.native()));
					continue;
				}
				SetFileAttributesW(f->name.c_str(), FILE_ATTRIBUTE_NORMAL);
				if (DeleteFileW(f->name.c_str()))
				{
					cbx::LOG("Deleted {}", cbx::ToUtf8(f->name.native()));
					deleted++;
				}
				else
				{
					cbx::LOG("Failed to delete source file: {}", cbx::ToUtf8(f->name.native()));
				}
			}
			co_await app::ShowMessageAsync(root, L"Delete source files", hstring(std::format(L"Deleted {} file(s)", deleted)));
		}
		dialogOpen = false;
	}

	fire_and_forget MainWindow::OnDeleteOutputFiles(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = SelectedFiles();
		auto root = Content().XamlRoot();
		auto lifetime = get_strong();
		if (files.empty() || dialogOpen)
			co_return;
		dialogOpen = true;
		bool confirmed = co_await app::ShowMessageAsync(root, L"Delete output files",
			hstring(std::format(L"Delete {} output file(s)?", files.size())), L"Delete", L"Cancel");
		if (confirmed)
		{
			int deleted = 0;
			for (const auto& f : files)
			{
				fs::path out;
				{
					std::lock_guard lock(f->mutex);
					out = f->outFile;
				}
				if (out.empty())
					continue;
				SetFileAttributesW(out.c_str(), FILE_ATTRIBUTE_NORMAL);
				if (DeleteFileW(out.c_str()))
				{
					cbx::LOG("Deleted {}", cbx::ToUtf8(out.native()));
					deleted++;
				}
				else
				{
					cbx::LOG("Failed to delete output file: {}", cbx::ToUtf8(out.native()));
				}
			}
			co_await app::ShowMessageAsync(root, L"Delete output files", hstring(std::format(L"Deleted {} file(s)", deleted)));
		}
		dialogOpen = false;
	}

	void MainWindow::OnClearConverted(IInspectable const&, RoutedEventArgs const&)
	{
		std::vector<CbxConverter::SourceFileItem> toRemove;
		for (auto const& item : items)
		{
			auto file = Native(item)->File();
			if (file->state == cbx::SourceFile::DONE && file->jobCount == 0)
				toRemove.push_back(item);
		}
		RemoveItems(toRemove);
	}

	// ---------------------------------------------------------------- conversion

	void MainWindow::OnStartConversion(IInspectable const&, RoutedEventArgs const&)
	{
		auto& st = app::AppState::Get();
		auto cfg = st.MakeJobConfig();
		int queued = 0;
		for (auto const& item : items)
		{
			auto file = Native(item)->File();
			auto state = file->state.load();
			if (state == cbx::SourceFile::DONE || state == cbx::SourceFile::SOURCE_ERROR || file->convertQueued)
				continue;
			st.engine->AddConvert(file, cfg);
			queued++;
		}
		if (queued)
			cbx::LOG("Queued {} file(s) for conversion to {}, quality {}", queued,
				cbx::ToUtf8(cbx::Settings::Conversion::FormatName(cfg.encode.format)), cfg.encode.quality);
		RefreshView();
	}

	void MainWindow::OnStop(IInspectable const&, RoutedEventArgs const&)
	{
		cbx::LOG("Stopping...");
		app::AppState::Get().engine->AbortAll();
	}

	// ---------------------------------------------------------------- menu

	void MainWindow::OnOpenTmpDirectory(IInspectable const&, RoutedEventArgs const&)
	{
		app::ShellOpen(app::AppState::Get().tmpPath.wstring());
	}

	void MainWindow::OnOpenOutputDirectory(IInspectable const&, RoutedEventArgs const&)
	{
		app::ShellOpen(app::AppState::Get().outPath.wstring());
	}

	void MainWindow::OnExit(IInspectable const&, RoutedEventArgs const&)
	{
		Close();
	}

	void MainWindow::OnShowLog(IInspectable const&, RoutedEventArgs const&)
	{
		if (!logWindow)
		{
			logWindow = make<LogWindow>();
			logWindow.Closed([this](auto&&, auto&&) { logWindow = nullptr; });
		}
		logWindow.Activate();
	}

	void MainWindow::OnShowSettings(IInspectable const&, RoutedEventArgs const&)
	{
		if (dialogOpen)
			return;
		dialogOpen = true;
		auto& st = app::AppState::Get();
		auto window = make<SettingsWindow>();
		get_self<SettingsWindow>(window)->Show(app::GetWindowHandle(*this), st.settings, [this, lifetime = get_strong()](bool applied) {
			dialogOpen = false;
			if (!applied)
				return;
			auto& st = app::AppState::Get();
			st.ApplySettings();
			st.SaveSettings();
			if (auto presenter = AppWindow().Presenter().try_as<OverlappedPresenter>())
				presenter.IsAlwaysOnTop(st.settings.mainWindow.alwaysOnTop);
		});
	}

	fire_and_forget MainWindow::OnAbout(IInspectable const&, RoutedEventArgs const&)
	{
		if (dialogOpen)
			co_return;
		auto lifetime = get_strong();
		auto dialog = app::MakeDialog(Content().XamlRoot(), CBX_APP_NAME L" " CBX_VERSION_STRING);
		StackPanel panel;
		panel.Spacing(8);
		auto addText = [&](const std::wstring& text, double opacity = 1.0) {
			TextBlock tb;
			tb.Text(text);
			tb.TextWrapping(TextWrapping::Wrap);
			tb.Opacity(opacity);
			tb.IsTextSelectionEnabled(true);
			panel.Children().Append(tb);
		};
		addText(L"Converter for cbr / cbz / cb7 / cbt and pdf comic book files.");
		addText(L"Modified version of CbxConverter by Tomasz Ostrowski: ported to Visual Studio / WinUI 3, "
			L"in-process image conversion using all CPU cores.");
		addText(L"Modified by Fabian Korak (github.com/fkorak). Developed with AI assistance (Claude Code by Anthropic).");
		addText(L"Original program: Copyright © Tomasz Ostrowski 2013-2026. Freeware, GPL v2.");
		HyperlinkButton link;
		link.Content(box_value(CBX_ORIGINAL_URL));
		link.NavigateUri(Windows::Foundation::Uri(CBX_ORIGINAL_URL));
		link.Padding(ThicknessHelper::FromLengths(0, 0, 0, 0));
		panel.Children().Append(link);
		addText(cbx::FromUtf8(cbx::CodecLibraryVersions()), 0.7);
		addText(cbx::FromUtf8(cbx::ArchiveLibraryVersion()), 0.7);
		addText(L"Built " __DATE__ " " __TIME__, 0.7);
		dialog.Content(panel);
		dialog.CloseButtonText(L"OK");
		dialogOpen = true;
		co_await dialog.ShowAsync();
		dialogOpen = false;
	}
}
