#include "pch.h"
#include "SettingsControl.xaml.h"
#if __has_include("SettingsControl.g.cpp")
#include "SettingsControl.g.cpp"
#endif

#include "Archive.h"
#include "Dialogs.h"

#include <thread>

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::CbxConverter::implementation
{
	namespace
	{
		int ToInt(NumberBox const& box, int def)
		{
			double v = box.Value();
			return std::isnan(v) ? def : static_cast<int>(v + 0.5);
		}

		const int logLineOptions[] = { 100, 200, 500, 1000, 2000, 5000, 10000 };

		std::wstring Text(TextBox const& box)
		{
			return std::wstring(box.Text());
		}
	}

	void SettingsControl::Load(const cbx::Settings& s, HWND ownerWindow)
	{
		owner = ownerWindow;

		// general
		ThreadCountBox().Value(s.worker.threadCount);
		ThreadCountHint().Text(std::format(L"This computer has {} logical processors. Using all of them gives the fastest conversion; "
			L"lower the number to keep the computer responsive.", std::thread::hardware_concurrency()));
		PriorityCombo().SelectedIndex(s.worker.priority);
		PlaySoundCheck().IsChecked(s.worker.playSoundWhenDone);
		AlwaysOnTopCheck().IsChecked(s.mainWindow.alwaysOnTop);

		// conversion
		FormatCombo().SelectedIndex(s.conversion.format);
		QualityBox().Value(s.conversion.quality);
		WebpMethodSlider().Value(s.conversion.webpMethod);
		WebpLosslessCheck().IsChecked(s.conversion.webpLossless);
		KeepOriginalCheck().IsChecked(s.conversion.keepOriginalIfSmaller);
		FilesToSkipBox().Text(s.conversion.filesToSkip);
		maxRules = static_cast<unsigned int>(s.conversion.defResize.size());
		for (const auto& r : s.conversion.defResize)
		{
			if (r.width > 0)
				AddRuleRow(r.width, r.resize);
		}
		OnFormatChanged(nullptr, nullptr);

		// archives & pdf
		UnpackPasswordBox().Password(s.conversion.unpackPassword);
		CopyOnErrorCheck().IsChecked(s.conversion.copyFileToOutputOnError);
		SevenZipBox().Text(s.tools.sevenZipLocation);
		SevenZipBox().TextChanged([this](auto&&, auto&&) { Update7zHint(); });
		Update7zHint();
		GsLocationBox().Text(s.pdfImport.gsLocation);
		GsParamsBox().Text(s.pdfImport.gsParams);
		GsPatternBox().Text(s.pdfImport.gsFilePattern);

		// directories
		for (int i = 0; i < cbx::Settings::Directories::DirectoryTypeLimiter; i++)
		{
			auto desc = box_value(cbx::Settings::Directories::DirectoryTypeDescription(static_cast<cbx::Settings::Directories::DirectoryType>(i)));
			TmpDirTypeCombo().Items().Append(desc);
			OutDirTypeCombo().Items().Append(box_value(cbx::Settings::Directories::DirectoryTypeDescription(static_cast<cbx::Settings::Directories::DirectoryType>(i))));
		}
		TmpDirTypeCombo().SelectedIndex(s.directories.tmpDirectoryType);
		TmpDirBox().Text(s.directories.customTmpDirectory);
		OutDirTypeCombo().SelectedIndex(s.directories.outDirectoryType);
		OutDirBox().Text(s.directories.customOutDirectory);
		UseSourceDirCheck().IsChecked(s.directories.useSourceDirectoryForOutput);
		RecreateDirsCheck().IsChecked(s.directories.recreateSourceDirectoryForOutput);
		NumericTmpCheck().IsChecked(s.directories.useNumericTmpFileDirectory);
		UpdateDirectoriesPage();

		// logging
		LogToFileCheck().IsChecked(s.logging.logToFile);
		int logIndex = static_cast<int>(std::size(logLineOptions)) - 1;
		for (int i = 0; i < static_cast<int>(std::size(logLineOptions)); i++)
		{
			if (static_cast<unsigned int>(logLineOptions[i]) >= s.logging.maxUiLogLines)
			{
				logIndex = i;
				break;
			}
		}
		LogLinesCombo().SelectedIndex(logIndex);
	}

	void SettingsControl::Save(cbx::Settings& s)
	{
		s.worker.threadCount = std::clamp(ToInt(ThreadCountBox(), s.worker.threadCount),
			cbx::Settings::Worker::THREAD_COUNT_MIN, cbx::Settings::Worker::THREAD_COUNT_MAX);
		s.worker.priority = static_cast<cbx::Settings::Worker::Priority>(std::max(0, PriorityCombo().SelectedIndex()));
		s.worker.playSoundWhenDone = PlaySoundCheck().IsChecked().Value();
		s.mainWindow.alwaysOnTop = AlwaysOnTopCheck().IsChecked().Value();

		s.conversion.format = static_cast<cbx::Settings::Conversion::Format>(std::max(0, FormatCombo().SelectedIndex()));
		s.conversion.quality = std::clamp(ToInt(QualityBox(), s.conversion.quality), 1, 100);
		s.conversion.webpMethod = std::clamp(static_cast<int>(WebpMethodSlider().Value()), 0, 6);
		s.conversion.webpLossless = WebpLosslessCheck().IsChecked().Value();
		s.conversion.keepOriginalIfSmaller = KeepOriginalCheck().IsChecked().Value();
		s.conversion.filesToSkip = Text(FilesToSkipBox());
		for (auto& r : s.conversion.defResize)
			r = {};
		size_t n = 0;
		for (const auto& row : rules)
		{
			int w = ToInt(row.width, 0);
			int p = ToInt(row.pct, 100);
			if (w > 0 && n < s.conversion.defResize.size())
				s.conversion.defResize[n++] = { w, p };
		}

		s.conversion.unpackPassword = std::wstring(UnpackPasswordBox().Password());
		s.conversion.copyFileToOutputOnError = CopyOnErrorCheck().IsChecked().Value();
		s.tools.sevenZipLocation = Text(SevenZipBox());
		s.pdfImport.gsLocation = Text(GsLocationBox());
		s.pdfImport.gsParams = Text(GsParamsBox());
		s.pdfImport.gsFilePattern = Text(GsPatternBox());

		s.directories.tmpDirectoryType = static_cast<cbx::Settings::Directories::DirectoryType>(std::max(0, TmpDirTypeCombo().SelectedIndex()));
		s.directories.customTmpDirectory = Text(TmpDirBox());
		s.directories.outDirectoryType = static_cast<cbx::Settings::Directories::DirectoryType>(std::max(0, OutDirTypeCombo().SelectedIndex()));
		s.directories.customOutDirectory = Text(OutDirBox());
		s.directories.useSourceDirectoryForOutput = UseSourceDirCheck().IsChecked().Value();
		s.directories.recreateSourceDirectoryForOutput = RecreateDirsCheck().IsChecked().Value();
		s.directories.useNumericTmpFileDirectory = NumericTmpCheck().IsChecked().Value();

		s.logging.logToFile = LogToFileCheck().IsChecked().Value();
		int logIndex = LogLinesCombo().SelectedIndex();
		if (logIndex >= 0)
			s.logging.maxUiLogLines = logLineOptions[logIndex];
	}

	void SettingsControl::OnSectionChanged(SelectorBar const& sender, SelectorBarSelectionChangedEventArgs const&)
	{
		if (!PageLogging())	// raised while XAML is still loading
			return;
		int index = 0;
		if (auto item = sender.SelectedItem())
			index = std::stoi(unbox_value<hstring>(item.Tag()).c_str());
		UIElement pages[] = { PageGeneral(), PageConversion(), PageResizing(), PageArchives(), PageDirectories(), PageLogging() };
		for (int i = 0; i < static_cast<int>(std::size(pages)); i++)
			pages[i].Visibility(i == index ? Visibility::Visible : Visibility::Collapsed);
	}

	void SettingsControl::OnFormatChanged(IInspectable const&, SelectionChangedEventArgs const&)
	{
		int format = FormatCombo().SelectedIndex();
		auto webp = format == cbx::Settings::Conversion::FormatWebp ? Visibility::Visible : Visibility::Collapsed;
		WebpOptions().Visibility(webp);
		WebpMethodSlider().Visibility(webp);
		QualityBox().IsEnabled(format != cbx::Settings::Conversion::FormatPng);
	}

	void SettingsControl::AddRuleRow(int width, int pct)
	{
		RuleRow row;
		row.grid = Grid();
		row.grid.ColumnSpacing(8);
		row.grid.Width(420);
		row.grid.HorizontalAlignment(HorizontalAlignment::Left);
		for (auto w : { GridLengthHelper::FromValueAndType(1, GridUnitType::Star), GridLengthHelper::FromValueAndType(1, GridUnitType::Star), GridLengthHelper::FromPixels(40) })
		{
			ColumnDefinition cd;
			cd.Width(w);
			row.grid.ColumnDefinitions().Append(cd);
		}
		row.width = NumberBox();
		row.width.Minimum(0);
		row.width.Maximum(100000);
		row.width.Value(width);
		row.pct = NumberBox();
		row.pct.Minimum(1);
		row.pct.Maximum(999);
		row.pct.Value(pct);
		Grid::SetColumn(row.pct, 1);
		Button remove;
		FontIcon icon;
		icon.Glyph(L"\xE74D");
		icon.FontSize(12);
		remove.Content(icon);
		ToolTipService::SetToolTip(remove, box_value(L"Remove rule"));
		Grid::SetColumn(remove, 2);
		auto grid = row.grid;
		remove.Click([this, grid](auto&&, auto&&) {
			auto it = std::find_if(rules.begin(), rules.end(), [&](const RuleRow& r) { return r.grid == grid; });
			if (it != rules.end())
				rules.erase(it);
			uint32_t index;
			if (ResizeRulesPanel().Children().IndexOf(grid, index))
				ResizeRulesPanel().Children().RemoveAt(index);
			AddRuleButton().IsEnabled(rules.size() < maxRules);
		});
		row.grid.Children().Append(row.width);
		row.grid.Children().Append(row.pct);
		row.grid.Children().Append(remove);
		ResizeRulesPanel().Children().Append(row.grid);
		rules.push_back(row);
		AddRuleButton().IsEnabled(rules.size() < maxRules);
	}

	void SettingsControl::OnAddRule(IInspectable const&, RoutedEventArgs const&)
	{
		if (rules.size() < maxRules)
			AddRuleRow(0, 50);
	}

	void SettingsControl::Update7zHint()
	{
		auto found = cbx::Find7z(Text(SevenZipBox()));
		SevenZipHint().Text(found.empty()
			? hstring(L"7-Zip not found. Archives are unpacked with built-in libarchive; 7z.exe is only used as a fallback (e.g. for encrypted RAR files).")
			: hstring(L"Fallback for archives libarchive cannot read: " + found.wstring()));
	}

	void SettingsControl::OnBrowse7z(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = app::PickFiles(owner, { { L"7z.exe", L"7z.exe" }, { L"Executable (*.exe)", L"*.exe" } }, false, Text(SevenZipBox()));
		if (!files.empty())
			SevenZipBox().Text(files[0]);
	}

	void SettingsControl::OnBrowseGs(IInspectable const&, RoutedEventArgs const&)
	{
		auto files = app::PickFiles(owner, { { L"Executable (*.exe)", L"*.exe" }, { L"All files", L"*.*" } }, false, Text(GsLocationBox()));
		if (!files.empty())
			GsLocationBox().Text(files[0]);
	}

	void SettingsControl::OnBrowseTmpDir(IInspectable const&, RoutedEventArgs const&)
	{
		auto dir = app::PickFolder(owner, Text(TmpDirBox()));
		if (!dir.empty())
			TmpDirBox().Text(dir);
	}

	void SettingsControl::OnBrowseOutDir(IInspectable const&, RoutedEventArgs const&)
	{
		auto dir = app::PickFolder(owner, Text(OutDirBox()));
		if (!dir.empty())
			OutDirBox().Text(dir);
	}

	void SettingsControl::OnDirectoryOptionChanged(IInspectable const&, IInspectable const&)
	{
		UpdateDirectoriesPage();
	}

	void SettingsControl::UpdateDirectoriesPage()
	{
		bool customTmp = TmpDirTypeCombo().SelectedIndex() == cbx::Settings::Directories::DirectoryTypeCustom;
		TmpDirRow().Visibility(customTmp ? Visibility::Visible : Visibility::Collapsed);
		bool customOut = OutDirTypeCombo().SelectedIndex() == cbx::Settings::Directories::DirectoryTypeCustom;
		OutDirRow().Visibility(customOut ? Visibility::Visible : Visibility::Collapsed);

		bool useSource = UseSourceDirCheck().IsChecked().Value();
		OutDirTypeCombo().IsEnabled(!useSource);
		OutDirBox().IsEnabled(!useSource);
		OutDirBrowseButton().IsEnabled(!useSource);
		RecreateDirsCheck().Visibility(useSource ? Visibility::Collapsed : Visibility::Visible);
	}
}
