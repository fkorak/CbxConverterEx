#include "Engine.h"
#include "Archive.h"
#include "FileNameMatch.h"
#include "Log.h"
#include "Process.h"
#include "StringUtils.h"

#include <windows.h>
#include <algorithm>
#include <format>

namespace fs = std::filesystem;

namespace cbx
{

namespace
{

int ThreadPriority(Settings::Worker::Priority p)
{
	switch (p)
	{
	case Settings::Worker::PriorityIdle:
		return THREAD_PRIORITY_IDLE;
	case Settings::Worker::PriorityNormal:
		return THREAD_PRIORITY_NORMAL;
	case Settings::Worker::PriorityAboveNormal:
		return THREAD_PRIORITY_ABOVE_NORMAL;
	default:
		return THREAD_PRIORITY_BELOW_NORMAL;
	}
}

void RemoveDirectoryTree(const fs::path& dir)
{
	std::error_code ec;
	if (!fs::exists(dir, ec))
		return;
	// clear read-only attributes (copied from archives) that would block removal
	for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
		SetFileAttributesW(it->path().c_str(), FILE_ATTRIBUTE_NORMAL);
	fs::remove_all(dir, ec);
	if (ec)
		LOG("Failed to delete directory {}: {}", ToUtf8(dir.native()), ec.message());
}

std::vector<fs::path> ListFiles(const fs::path& dir)
{
	std::vector<fs::path> files;
	std::error_code ec;
	for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
	{
		if (it->is_regular_file(ec))
			files.push_back(it->path());
	}
	std::sort(files.begin(), files.end());
	return files;
}

uint64_t FileSize(const fs::path& p)
{
	std::error_code ec;
	auto s = fs::file_size(p, ec);
	return ec ? 0 : s;
}

}	// namespace

JobConfig JobConfig::FromSettings(const Settings& s, const fs::path& outDir)
{
	JobConfig c;
	c.outDir = outDir;
	c.useSourceDirectoryForOutput = s.directories.useSourceDirectoryForOutput;
	c.recreateSourceDirectoryForOutput = s.directories.recreateSourceDirectoryForOutput;
	c.copyFileToOutputOnError = s.conversion.copyFileToOutputOnError;
	c.unpackPassword = s.conversion.unpackPassword;
	c.sevenZip = Find7z(s.tools.sevenZipLocation);
	c.gsLocation = s.pdfImport.gsLocation;
	c.gsParams = s.pdfImport.gsParams;
	c.gsFilePattern = s.pdfImport.gsFilePattern;
	c.filesToSkip = Trim(s.conversion.filesToSkip);
	c.defResize = s.conversion.defResize;
	c.encode.format = s.conversion.format;
	c.encode.quality = s.conversion.quality;
	c.encode.webpMethod = s.conversion.webpMethod;
	c.encode.webpLossless = s.conversion.webpLossless;
	c.encode.keepOriginalIfSmaller = s.conversion.keepOriginalIfSmaller;
	c.priorityClass = Engine::PriorityClass(s.worker.priority);
	return c;
}

unsigned long Engine::PriorityClass(Settings::Worker::Priority p)
{
	switch (p)
	{
	case Settings::Worker::PriorityIdle:
		return IDLE_PRIORITY_CLASS;
	case Settings::Worker::PriorityNormal:
		return NORMAL_PRIORITY_CLASS;
	case Settings::Worker::PriorityAboveNormal:
		return ABOVE_NORMAL_PRIORITY_CLASS;
	default:
		return BELOW_NORMAL_PRIORITY_CLASS;
	}
}

Engine::Engine(int threadCount, Settings::Worker::Priority priority)
{
	StartThreads(threadCount, priority);
}

Engine::~Engine()
{
	AbortAll();
	StopThreads();
}

void Engine::StartThreads(int count, Settings::Worker::Priority priority)
{
	threadCount = std::max(1, count);
	threadPriority = ThreadPriority(priority);
	pool = std::make_unique<ThreadPool>(threadCount, threadPriority);
	// archive-level parallelism: unpacking and packing are mostly single-threaded / IO bound
	int pipelineCount = std::clamp(threadCount / 2, 2, 8);
	stopping = false;
	for (int i = 0; i < pipelineCount; i++)
		pipelines.emplace_back(&Engine::PipelineThread, this);
}

void Engine::StopThreads()
{
	{
		std::lock_guard lock(mutex);
		stopping = true;
	}
	cv.notify_all();
	for (auto& t : pipelines)
		t.join();
	pipelines.clear();
	pool.reset();
}

bool Engine::Reconfigure(int count, Settings::Worker::Priority priority)
{
	if (count == threadCount && ThreadPriority(priority) == threadPriority)
		return true;
	if (!IsIdle())
		return false;
	StopThreads();
	StartThreads(count, priority);
	LOG("Engine: {} worker threads", threadCount);
	return true;
}

int Engine::GetThreadCount() const
{
	return threadCount;
}

void Engine::AddUnpack(const std::shared_ptr<SourceFile>& file, const JobConfig& cfg)
{
	file->jobCount++;
	{
		std::lock_guard lock(mutex);
		queue.push_back({ JobType::Unpack, file, cfg });
	}
	cv.notify_one();
}

void Engine::AddConvert(const std::shared_ptr<SourceFile>& file, const JobConfig& cfg)
{
	if (file->convertQueued.exchange(true))
		return;
	file->jobCount++;
	{
		std::lock_guard lock(mutex);
		queue.push_back({ JobType::Convert, file, cfg });
	}
	cv.notify_one();
}

void Engine::AbortAll()
{
	std::deque<Job> dropped;
	{
		std::lock_guard lock(mutex);
		dropped.swap(queue);
		if (running > 0)
			abort = true;
	}
	for (auto& job : dropped)
	{
		if (job.type == JobType::Convert)
			job.file->convertQueued = false;
		job.file->jobCount--;
	}
}

bool Engine::IsIdle() const
{
	std::lock_guard lock(mutex);
	return queue.empty() && running == 0;
}

void Engine::PipelineThread()
{
	SetThreadPriority(GetCurrentThread(), threadPriority);
	for (;;)
	{
		Job job;
		{
			std::unique_lock lock(mutex);
			std::deque<Job>::iterator it;
			cv.wait(lock, [&] {
				if (stopping)
					return true;
				// jobs for the same file must not run concurrently (e.g. convert queued while unpacking)
				it = std::find_if(queue.begin(), queue.end(), [](const Job& j) { return !j.file->busy; });
				return it != queue.end();
			});
			if (stopping)
				return;
			job = std::move(*it);
			queue.erase(it);
			job.file->busy = true;
			running++;
		}

		Execute(job);

		{
			std::lock_guard lock(mutex);
			if (job.type == JobType::Convert)
				job.file->convertQueued = false;
			job.file->busy = false;
			job.file->jobCount--;
			running--;
			if (running == 0)
				abort = false;
		}
		cv.notify_all();
	}
}

void Engine::Execute(Job& job)
{
	SourceFile& file = *job.file;
	if (file.state == SourceFile::DONE)
		return;
	if (job.type == JobType::Unpack)
	{
		Unpack(job);
		if (file.state != SourceFile::SOURCE_ERROR && !abort)
			Identify(job);
	}
	else
	{
		bool unpacked;
		{
			std::lock_guard lock(file.mutex);
			unpacked = file.unpacked;
		}
		if (!unpacked)
		{
			LOG("Skipping {}: not unpacked", ToUtf8(file.name.native()));
			return;
		}
		Convert(job);
		if (!abort && file.state != SourceFile::SOURCE_ERROR)
		{
			bool doNotPack;
			{
				std::lock_guard lock(file.mutex);
				doNotPack = file.doNotPack;
			}
			if (doNotPack)
				file.state = SourceFile::DONE;
			else
				Pack(job);
		}
	}

	if (file.state != SourceFile::DONE && file.state != SourceFile::SOURCE_ERROR)
		file.state = SourceFile::IDLE;
	file.stateProgress = -1;
	if (file.state == SourceFile::SOURCE_ERROR)
		CopyOnError(job);
}

void Engine::Unpack(Job& job)
{
	SourceFile& file = *job.file;
	const fs::path& dir = file.tmpDir;
	RemoveDirectoryTree(dir);
	std::error_code ec;
	fs::create_directories(dir, ec);
	if (ec)
	{
		LOG("Failed to create tmp dir {}: {}", ToUtf8(dir.native()), ec.message());
		file.state = SourceFile::SOURCE_ERROR;
		return;
	}

	std::string error;
	bool ok;
	if (file.isPdf)
	{
		file.state = SourceFile::RENDERING_PDF;
		fs::path pattern = dir / job.cfg.gsFilePattern;
		std::wstring cmd = QuoteArg(job.cfg.gsLocation.native()) + L" -q -dBATCH -dNOPAUSE " + job.cfg.gsParams +
			L" " + QuoteArg(L"-sOutputFile=" + pattern.native()) + L" " + QuoteArg(file.name.native());
		int rc = RunProcess(cmd, job.cfg.priorityClass, &abort);
		ok = rc == 0;
		if (!ok)
			error = std::format("Ghostscript failed with exit code {}", rc);
	}
	else
	{
		file.state = SourceFile::UNPACKING;
		ok = ExtractArchive(file.name, dir, job.cfg.unpackPassword, abort, error);
		if (!ok && !abort && !job.cfg.sevenZip.empty())
		{
			LOG("libarchive could not unpack {} ({}), retrying with 7z.exe", ToUtf8(file.name.native()), error);
			RemoveDirectoryTree(dir);
			fs::create_directories(dir, ec);
			error.clear();
			ok = ExtractArchive7z(job.cfg.sevenZip, file.name, dir, job.cfg.unpackPassword, job.cfg.priorityClass, abort, error);
		}
	}
	if (abort)
	{
		file.state = SourceFile::IDLE;
		return;
	}
	if (!ok)
	{
		LOG("Error unpacking {}: {}", ToUtf8(file.name.native()), error);
		file.state = SourceFile::SOURCE_ERROR;
	}
}

void Engine::Identify(Job& job)
{
	SourceFile& file = *job.file;
	file.state = SourceFile::IDENTIFYING;
	file.stateProgress = 0;

	std::vector<fs::path> files = ListFiles(file.tmpDir);
	std::vector<SourceFile::FileDesc> descs(files.size());
	std::atomic<size_t> done = 0;
	TaskGroup group;
	group.Add(static_cast<int>(files.size()));
	for (size_t i = 0; i < files.size(); i++)
	{
		pool->Submit([&, i] {
			auto& d = descs[i];
			d.name = files[i];
			d.size = FileSize(files[i]);
			if (!ReadImageInfo(files[i], d.width, d.height))
				d.width = d.height = -1;
			file.stateProgress = static_cast<int>(100 * ++done / files.size());
			group.Done();
		});
	}
	group.Wait();

	std::map<int, int> widths;
	unsigned int imgCount = 0;
	uint64_t imgSize = 0;
	for (const auto& d : descs)
	{
		if (d.IsImage())
		{
			imgCount++;
			imgSize += d.size;
			widths[d.width]++;
		}
	}
	std::vector<std::pair<int, int>> v(widths.begin(), widths.end());
	std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

	std::lock_guard lock(file.mutex);
	file.fileDescs = std::move(descs);
	file.imgCount = imgCount;
	file.imgSize = imgSize;
	if (v.empty())
		file.asWidth = L"?";
	else if (v.size() == 1)
		file.asWidth = std::format(L"{}px", v[0].first);
	else
		file.asWidth = std::format(L"{}px ({}), {}px ({})", v[0].first, v[0].second, v[1].first, v[1].second);
	file.mostFrequentWidth = v.empty() ? 0 : v[0].first;
	file.widthValid = true;
	file.unpacked = true;
	if (!file.defaultResizeApplied)
	{
		file.defaultResizeApplied = true;
		for (const auto& r : job.cfg.defResize)
		{
			if (r.width > 0 && r.width == file.mostFrequentWidth && r.resize > 0)
			{
				file.resizeCfg.mode = ResizeCfg::ModeRegular;
				file.resizeCfg.resizePct = r.resize;
				break;
			}
		}
	}
	LOG("{}: {} files, {} images", ToUtf8(file.name.filename().native()), file.fileDescs.size(), imgCount);
}

void Engine::Convert(Job& job)
{
	SourceFile& file = *job.file;
	file.state = SourceFile::CONVERTING;
	file.stateProgress = 0;

	std::vector<SourceFile::FileDesc> descs;
	ResizeCfg resizeCfg;
	{
		std::lock_guard lock(file.mutex);
		descs = file.fileDescs;
		resizeCfg = file.resizeCfg;
	}

	std::vector<size_t> todo;
	for (size_t i = 0; i < descs.size(); i++)
	{
		const auto& d = descs[i];
		if (!d.IsImage() || d.converted)
			continue;
		if (!job.cfg.filesToSkip.empty() && MatchMultiplePatterns(d.name.filename().native(), job.cfg.filesToSkip))
		{
			LOG("Skipping conversion of {}", ToUtf8(d.name.filename().native()));
			continue;
		}
		todo.push_back(i);
	}

	std::atomic<size_t> done = 0;
	std::atomic<unsigned int> errors = 0;
	TaskGroup group;
	group.Add(static_cast<int>(todo.size()));
	for (size_t idx : todo)
	{
		pool->Submit([&, idx] {
			if (!abort)
			{
				auto& d = descs[idx];
				int w = 0, h = 0;
				if (!resizeCfg.GetTargetSize(d.width, d.height, w, h))
					w = h = 0;
				ConvertResult r = ConvertImage(d.name, w, h, job.cfg.encode);
				if (r.ok)
				{
					d.name = r.outPath;
					d.width = r.width;
					d.height = r.height;
					d.size = r.size;
					d.converted = true;
				}
				else
				{
					errors++;
					LOG("Error converting {}: {}", ToUtf8(d.name.native()), r.error);
				}
			}
			file.stateProgress = static_cast<int>(100 * ++done / todo.size());
			group.Done();
		});
	}
	group.Wait();

	std::lock_guard lock(file.mutex);
	file.fileDescs = std::move(descs);
	file.convertErrors = errors;
	file.imgSize = 0;
	for (const auto& d : file.fileDescs)
	{
		if (d.IsImage())
			file.imgSize += d.size;
	}
}

fs::path Engine::GetOutDir(const Job& job) const
{
	const SourceFile& file = *job.file;
	if (job.cfg.useSourceDirectoryForOutput)
		return file.name.parent_path();
	fs::path dir = job.cfg.outDir;
	if (job.cfg.recreateSourceDirectoryForOutput && !file.rootPath.empty())
	{
		std::error_code ec;
		fs::path delta = fs::relative(file.name.parent_path(), file.rootPath, ec);
		if (!ec && !delta.empty() && delta != L".")
			dir /= delta;
	}
	std::error_code ec;
	fs::create_directories(dir, ec);
	if (ec)
		LOG("ERROR: Failed to create output directory ({}): {}", ToUtf8(dir.native()), ec.message());
	return dir;
}

void Engine::Pack(Job& job)
{
	SourceFile& file = *job.file;
	file.state = SourceFile::PACKING;
	fs::path outName = file.name.filename();
	if (job.cfg.useSourceDirectoryForOutput)
		outName.replace_extension(std::wstring(Settings::Conversion::FormatExtension(job.cfg.encode.format)) + L".cbz");
	else
		outName.replace_extension(L".cbz");
	fs::path out = GetOutDir(job) / outName;
	{
		std::lock_guard lock(file.mutex);
		file.outFile = out;
	}

	std::string error;
	if (!CreateZip(out, file.tmpDir, abort, error))
	{
		if (!abort)
		{
			LOG("Error packing {}: {}", ToUtf8(out.native()), error);
			file.state = SourceFile::SOURCE_ERROR;
		}
		return;
	}
	RemoveDirectoryTree(file.tmpDir);
	{
		std::lock_guard lock(file.mutex);
		file.outSize = FileSize(out);
		file.outSizeValid = true;
		file.unpacked = false;
	}
	file.state = SourceFile::DONE;
	LOG("Created {}", ToUtf8(out.native()));
}

void Engine::CopyOnError(Job& job)
{
	if (!job.cfg.copyFileToOutputOnError || job.cfg.useSourceDirectoryForOutput)
		return;
	SourceFile& file = *job.file;
	fs::path out = GetOutDir(job) / file.name.filename();
	{
		std::lock_guard lock(file.mutex);
		file.outFile = out;
	}
	if (!CopyFileW(file.name.c_str(), out.c_str(), FALSE))
		LOG("Failed to copy {} to output directory", ToUtf8(file.name.native()));
}

}	// namespace cbx
