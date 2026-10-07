#include "pch.h"
#include "RenameControl.xaml.h"
#if __has_include("RenameControl.g.cpp")
#include "RenameControl.g.cpp"
#endif

#include "RenameItem.h"
#include "Dialogs.h"
#include "Log.h"
#include "StringUtils.h"

#include <cwctype>
#include <set>

namespace fs = std::filesystem;

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;

namespace winrt::CbxConverter::implementation
{
	namespace
	{
		enum RenameType
		{
			RenameExtractingNumberFromName = 0,
			RenameStartingFromNumber
		};
	}

	std::optional<std::wstring> RenameControl::FormatPattern(const std::wstring& pattern, int number)
	{
		std::wstring out;
		bool converted = false;
		for (size_t i = 0; i < pattern.size(); i++)
		{
			wchar_t c = pattern[i];
			if (c != L'%')
			{
				out.push_back(c);
				continue;
			}
			if (i + 1 < pattern.size() && pattern[i + 1] == L'%')
			{
				out.push_back(L'%');
				i++;
				continue;
			}
			// %[flags][width](d|i|u)
			size_t j = i + 1;
			while (j < pattern.size() && wcschr(L"-+ 0#", pattern[j]))
				j++;
			while (j < pattern.size() && std::iswdigit(pattern[j]))
				j++;
			if (j >= pattern.size() || !wcschr(L"diu", pattern[j]) || converted || j - i > 8)
				return std::nullopt;
			std::wstring spec = pattern.substr(i, j - i + 1);
			if (spec.back() == L'u' || spec.back() == L'i')
				spec.back() = L'd';
			wchar_t buf[64];
			swprintf(buf, std::size(buf), spec.c_str(), number);
			out += buf;
			converted = true;
			i = j;
		}
		if (!converted)
			return std::nullopt;
		return out;
	}

	void RenameControl::SetFile(std::shared_ptr<cbx::SourceFile> f, HWND ownerWindow)
	{
		file = std::move(f);
		owner = ownerWindow;
		items = single_threaded_observable_vector<CbxConverter::RenameItem>();
		FileList().ItemsSource(items);
		Rebuild();
	}

	fs::path RenameControl::GetNewName(const fs::path& name, int index)
	{
		int offset = std::isnan(OffsetBox().Value()) ? 0 : static_cast<int>(OffsetBox().Value());
		std::wstring pattern(PatternBox().Text());
		int number;
		if (RenameTypeCombo().SelectedIndex() == RenameExtractingNumberFromName)
		{
			// last group of digits in file name (without extension)
			std::wstring stem = name.stem().wstring();
			size_t end = stem.size();
			while (end > 0 && !std::iswdigit(stem[end - 1]))
				end--;
			size_t begin = end;
			while (begin > 0 && std::iswdigit(stem[begin - 1]))
				begin--;
			if (begin == end)
				return {};
			try
			{
				number = std::stoi(stem.substr(begin, end - begin));
			}
			catch (...)
			{
				return {};
			}
		}
		else
		{
			number = index;
		}
		auto formatted = FormatPattern(pattern, number + offset);
		if (!formatted || formatted->empty() || formatted->find_first_of(L"\\/:*?\"<>|") != std::wstring::npos)
			return {};
		return name.parent_path() / (*formatted + name.extension().wstring());
	}

	void RenameControl::Rebuild()
	{
		std::vector<CbxConverter::RenameItem> list;
		{
			std::lock_guard lock(file->mutex);
			for (const auto& d : file->fileDescs)
			{
				std::error_code ec;
				fs::path rel = fs::relative(d.name, file->tmpDir, ec);
				list.push_back(make<RenameItem>(hstring(ec ? d.name.filename().wstring() : rel.wstring()), d.size, d.width, d.height));
			}
		}
		items.ReplaceAll(list);
		UpdateNewNames();
	}

	void RenameControl::UpdateNewNames()
	{
		if (!file || !items)
			return;
		std::lock_guard lock(file->mutex);
		for (uint32_t i = 0; i < items.Size() && i < file->fileDescs.size(); i++)
		{
			const auto& d = file->fileDescs[i];
			fs::path newName = GetNewName(d.name, static_cast<int>(i));
			std::wstring text;
			if (newName.empty())
				text = L"(cannot create name)";
			else if (newName != d.name)
				text = newName.filename().wstring();
			get_self<RenameItem>(items.GetAt(i))->SetNewName(hstring(text));
		}
	}

	void RenameControl::OnOptionsChanged(IInspectable const&, IInspectable const&)
	{
		if (!file)
			return;
		ErrorBar().IsOpen(false);
		UpdateNewNames();
	}

	void RenameControl::OnOffsetChanged(NumberBox const&, NumberBoxValueChangedEventArgs const&)
	{
		if (!file)
			return;
		ErrorBar().IsOpen(false);
		UpdateNewNames();
	}

	void RenameControl::ShowError(const std::wstring& text)
	{
		ErrorBar().Message(text);
		ErrorBar().IsOpen(true);
	}

	void RenameControl::OnRename(IInspectable const&, RoutedEventArgs const&)
	{
		ErrorBar().IsOpen(false);
		std::vector<fs::path> newNames;
		{
			std::lock_guard lock(file->mutex);
			std::set<std::wstring> unique;
			for (size_t i = 0; i < file->fileDescs.size(); i++)
			{
				const auto& d = file->fileDescs[i];
				fs::path newName = GetNewName(d.name, static_cast<int>(i));
				if (newName.empty())
				{
					ShowError(L"Cannot rename files, cannot create new name for " + d.name.filename().wstring());
					return;
				}
				if (!unique.insert(cbx::ToLower(newName.native())).second)
				{
					ShowError(L"Cannot rename files, duplicated name found among new names: " + newName.filename().wstring());
					return;
				}
				newNames.push_back(newName);
			}

			// two passes: new names may collide with old names of other files
			std::vector<fs::path> tmpNames(newNames.size());
			for (size_t i = 0; i < file->fileDescs.size(); i++)
			{
				auto& d = file->fileDescs[i];
				if (newNames[i] == d.name)
					continue;
				tmpNames[i] = d.name;
				tmpNames[i] += L".cbxrename";
				if (!MoveFileW(d.name.c_str(), tmpNames[i].c_str()))
				{
					ShowError(L"Failed renaming " + d.name.filename().wstring());
					tmpNames[i].clear();
					newNames[i] = d.name;
				}
			}
			for (size_t i = 0; i < file->fileDescs.size(); i++)
			{
				if (tmpNames[i].empty())
					continue;
				auto& d = file->fileDescs[i];
				if (MoveFileW(tmpNames[i].c_str(), newNames[i].c_str()))
				{
					d.name = newNames[i];
				}
				else
				{
					ShowError(L"Failed renaming " + d.name.filename().wstring() + L" to " + newNames[i].filename().wstring());
					MoveFileW(tmpNames[i].c_str(), d.name.c_str());
				}
			}
		}
		cbx::LOG("Renamed files in {}", cbx::ToUtf8(file->tmpDir.native()));
		OffsetBox().Value(0);
		Rebuild();
	}

	void RenameControl::OnOpenFile(IInspectable const&, IInspectable const&)
	{
		int index = FileList().SelectedIndex();
		if (index < 0)
			return;
		fs::path name;
		{
			std::lock_guard lock(file->mutex);
			if (index < static_cast<int>(file->fileDescs.size()))
				name = file->fileDescs[index].name;
		}
		if (!name.empty())
			app::ShellOpen(name.wstring());
	}

	void RenameControl::OnDeleteFiles(IInspectable const&, RoutedEventArgs const&)
	{
		std::vector<uint32_t> selected;
		for (auto const& range : FileList().SelectedRanges())
		{
			for (int i = range.FirstIndex(); i <= range.LastIndex(); i++)
				selected.push_back(static_cast<uint32_t>(i));
		}
		std::sort(selected.rbegin(), selected.rend());
		{
			std::lock_guard lock(file->mutex);
			for (uint32_t index : selected)
			{
				if (index >= file->fileDescs.size())
					continue;
				const auto& d = file->fileDescs[index];
				SetFileAttributesW(d.name.c_str(), FILE_ATTRIBUTE_NORMAL);
				if (!DeleteFileW(d.name.c_str()))
					cbx::LOG("Failed to delete {}", cbx::ToUtf8(d.name.native()));
				file->fileDescs.erase(file->fileDescs.begin() + index);
			}
			file->imgCount = 0;
			file->imgSize = 0;
			for (const auto& d : file->fileDescs)
			{
				if (d.IsImage())
				{
					file->imgCount++;
					file->imgSize += d.size;
				}
			}
		}
		Rebuild();
	}
}
