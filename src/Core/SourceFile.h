#pragma once

#include "ResizeCfg.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace cbx
{

/** Single source archive / pdf and its processing state.
	State and progress are atomics, other mutable fields are guarded by mutex
	(read by UI thread, written by worker threads).
*/
struct SourceFile
{
	enum State
	{
		IDLE = 0,
		UNPACKING,
		RENDERING_PDF,
		IDENTIFYING,
		CONVERTING,
		PACKING,
		DONE,
		SOURCE_ERROR
	};
	std::atomic<State> state = IDLE;
	std::atomic<int> stateProgress = -1;	///< 0..100 or -1
	std::atomic<int> jobCount = 0;			///< queued + running jobs
	std::atomic<bool> busy = false;			///< job is being executed right now
	std::atomic<bool> convertQueued = false;	///< conversion job queued or running

	// set when file is added, constant afterwards
	std::filesystem::path name;
	std::filesystem::path rootPath;	///< directory added recursively (for recreating structure), may be empty
	std::filesystem::path tmpDir;
	uint64_t size = 0;
	bool isPdf = false;

	struct FileDesc
	{
		std::filesystem::path name;
		int width = -1;
		int height = -1;
		uint64_t size = 0;
		bool converted = false;	///< avoid re-encoding when conversion is restarted
		bool IsImage() const
		{
			return width > 0 && height > 0;
		}
	};

	mutable std::mutex mutex;
	// guarded by mutex
	ResizeCfg resizeCfg;
	bool doNotPack = false;
	bool unpacked = false;
	bool defaultResizeApplied = false;
	unsigned int imgCount = 0;
	uint64_t imgSize = 0;
	bool widthValid = false;
	std::wstring asWidth;
	int mostFrequentWidth = 0;
	bool outSizeValid = false;
	uint64_t outSize = 0;
	std::filesystem::path outFile;
	unsigned int convertErrors = 0;
	std::vector<FileDesc> fileDescs;

	std::wstring StateName() const;

	/** Name displayed in list: path relative to root directory if added recursively */
	std::wstring DisplayName() const;
};

}	// namespace cbx
