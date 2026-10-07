#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace cbx
{

/** Fixed-size pool of worker threads executing queued tasks in FIFO order */
class ThreadPool
{
public:
	ThreadPool(int threadCount, int threadPriority);
	~ThreadPool();
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	void Submit(std::function<void()> task);
	int GetThreadCount() const
	{
		return static_cast<int>(threads.size());
	}

private:
	void Run(int priority);
	std::mutex mutex;
	std::condition_variable cv;
	std::deque<std::function<void()>> tasks;
	std::vector<std::thread> threads;
	bool stopping = false;
};

/** Counts outstanding tasks, allows waiting until all of them are done */
class TaskGroup
{
public:
	void Add(int n = 1);
	void Done();
	void Wait();

private:
	std::mutex mutex;
	std::condition_variable cv;
	int pending = 0;
};

}	// namespace cbx
