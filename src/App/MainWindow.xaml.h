#pragma once

#include "MainWindow.g.h"
#include "SourceFile.h"

namespace winrt::CbxConverter::implementation
{
	struct MainWindow : MainWindowT<MainWindow>
	{
		MainWindow();
		void InitializeComponent();

		// menu
		void OnAddFiles(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnAddDirectory(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnOpenTmpDirectory(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnOpenOutputDirectory(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnExit(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnShowLog(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnShowSettings(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::fire_and_forget OnAbout(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

		// list
		void OnHeaderClick(IInspectable const& sender, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnFileListKeyDown(IInspectable const&, Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& e);
		void OnFileListRightTapped(IInspectable const&, Microsoft::UI::Xaml::Input::RightTappedRoutedEventArgs const& e);
		void OnFileListDoubleTapped(IInspectable const&, Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e);
		void OnFileListMenuOpening(IInspectable const&, IInspectable const&);
		winrt::fire_and_forget OnResize(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::fire_and_forget OnRename(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnSkipPacking(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnOpenItemTmp(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnShowOutputFile(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnSelectAll(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnRemoveFiles(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::fire_and_forget OnDeleteSourceFiles(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		winrt::fire_and_forget OnDeleteOutputFiles(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnClearConverted(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

		// column resizing
		CbxConverter::ColumnLayout Layout() const { return layout; }
		void OnGripperPressed(IInspectable const& sender, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		void OnGripperMoved(IInspectable const& sender, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		void OnGripperReleased(IInspectable const& sender, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		void OnGripperCaptureLost(IInspectable const& sender, Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
		void OnGripperDoubleTapped(IInspectable const& sender, Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e);

		// drag & drop
		void OnDragOver(IInspectable const&, Microsoft::UI::Xaml::DragEventArgs const& e);
		winrt::fire_and_forget OnDrop(IInspectable const&, Microsoft::UI::Xaml::DragEventArgs const& e);

		// bottom bar
		void OnStartConversion(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);
		void OnStop(IInspectable const&, Microsoft::UI::Xaml::RoutedEventArgs const&);

	private:
		void InitWindow();
		void ApplyWindowSettings();
		double DpiScale();
		void SaveWindowPlacement();
		winrt::fire_and_forget OnClosing(Microsoft::UI::Windowing::AppWindow const&, Microsoft::UI::Windowing::AppWindowClosingEventArgs const& args);

		void AddSourceFile(const std::filesystem::path& path, const std::filesystem::path& rootPath);
		void AddSourceDirectory(const std::filesystem::path& dir);
		void AddPaths(const std::vector<std::wstring>& paths);
		std::filesystem::path MakeTmpDir(const std::filesystem::path& file);

		void RefreshView();
		void Sort(int column);
		std::vector<CbxConverter::SourceFileItem> SelectedItems();
		std::vector<std::shared_ptr<cbx::SourceFile>> SelectedFiles();
		void RemoveItems(const std::vector<CbxConverter::SourceFileItem>& toRemove);

		Windows::Foundation::Collections::IObservableVector<CbxConverter::SourceFileItem> items;
		CbxConverter::ColumnLayout layout{ nullptr };
		int dragColumn = -1;
		double dragStartX = 0;
		double dragStartWidth = 0;
		Microsoft::UI::Dispatching::DispatcherQueueTimer refreshTimer{ nullptr };
		CbxConverter::LogWindow logWindow{ nullptr };
		bool wasIdle = true;
		bool closeConfirmed = false;
		bool dialogOpen = false;
		int sortColumn = -1;
		bool sortAscending = true;
		unsigned int numericTmpDirId = 0;
	};
}

namespace winrt::CbxConverter::factory_implementation
{
	struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
	{
	};
}
