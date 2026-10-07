#pragma once

#include "ImageCodec.h"
#include "Settings.h"
#include "SourceFile.h"
#include "ThreadPool.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace cbx
{

/** Processing parameters, snapshot taken from settings when job is queued */
struct JobConfig
{
	std::filesystem::path outDir;
	bool useSourceDirectoryForOutput = false;
	bool recreateSourceDirectoryForOutput = false;
	bool copyFileToOutputOnError = false;
	std::wstring unpackPassword;
	std::filesystem::path sevenZip;		///< empty if not available
	std::filesystem::path gsLocation;
	std::wstring gsParams;
	std::wstring gsFilePattern;
	std::wstring filesToSkip;
	std::array<Settings::Conversion::DefResize, 10> defResize;
	EncodeOptions encode;
	unsigned long priorityClass = 0;	///< for external processes (7z, Ghostscript)

	static JobConfig FromSettings(const Settings& s, const std::filesystem::path& outDir);
};

/** Conversion engine.

	Archives are processed by a few "pipeline" threads (unpack, identify, convert,
	pack). Individual images of all archives are converted in parallel by a shared
	thread pool, so all cores are used even when converting a single archive.
*/
class Engine
{
public:
	Engine(int threadCount, Settings::Worker::Priority priority);
	~Engine();

	/** Reconfigure thread count / priority; only applied when engine is idle */
	bool Reconfigure(int threadCount, Settings::Worker::Priority priority);

	void AddUnpack(const std::shared_ptr<SourceFile>& file, const JobConfig& cfg);
	void AddConvert(const std::shared_ptr<SourceFile>& file, const JobConfig& cfg);

	/** Drop queued jobs and request running ones to stop (non-blocking) */
	void AbortAll();

	bool IsIdle() const;
	int GetThreadCount() const;

	static unsigned long PriorityClass(Settings::Worker::Priority p);

private:
	enum class JobType
	{
		Unpack,
		Convert
	};
	struct Job
	{
		JobType type;
		std::shared_ptr<SourceFile> file;
		JobConfig cfg;
	};

	void PipelineThread();
	void Execute(Job& job);
	void Unpack(Job& job);
	void Identify(Job& job);
	void Convert(Job& job);
	void Pack(Job& job);
	void CopyOnError(Job& job);
	std::filesystem::path GetOutDir(const Job& job) const;
	void StartThreads(int threadCount, Settings::Worker::Priority priority);
	void StopThreads();

	mutable std::mutex mutex;
	std::condition_variable cv;
	std::deque<Job> queue;
	int running = 0;
	bool stopping = false;
	std::atomic<bool> abort = false;
	std::vector<std::thread> pipelines;
	std::unique_ptr<ThreadPool> pool;
	int threadPriority = 0;
	int threadCount = 1;
};

}	// namespace cbx
