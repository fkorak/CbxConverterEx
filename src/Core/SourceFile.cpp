#include "SourceFile.h"

#include <format>

namespace cbx
{

std::wstring SourceFile::StateName() const
{
	switch (state.load())
	{
	case IDLE:
		return jobCount == 0 ? L"Idle" : L"Waiting...";
	case UNPACKING:
		return L"Unpacking...";
	case RENDERING_PDF:
		return L"Rendering PDF...";
	case IDENTIFYING:
		return std::format(L"Identifying, {}%", stateProgress.load());
	case CONVERTING:
		return std::format(L"Converting, {}%", stateProgress.load());
	case PACKING:
		return L"Packing...";
	case DONE:
	{
		std::lock_guard lock(mutex);
		if (convertErrors)
			return std::format(L"Done ({} image(s) not converted)", convertErrors);
		return L"Done";
	}
	case SOURCE_ERROR:
		return L"Error processing file";
	default:
		return L"???";
	}
}

std::wstring SourceFile::DisplayName() const
{
	if (!rootPath.empty())
	{
		std::error_code ec;
		std::filesystem::path rel = std::filesystem::relative(name, rootPath, ec);
		if (!ec && !rel.empty())
			return rel.wstring();
	}
	return name.filename().wstring();
}

}	// namespace cbx
