#include "ThreadPool.h"

#include <windows.h>

namespace cbx
{

ThreadPool::ThreadPool(int threadCount, int threadPriority)
{
	if (threadCount < 1)
		threadCount = 1;
	threads.reserve(threadCount);
	for (int i = 0; i < threadCount; i++)
		threads.emplace_back(&ThreadPool::Run, this, threadPriority);
}

ThreadPool::~ThreadPool()
{
	{
		std::lock_guard lock(mutex);
		stopping = true;
	}
	cv.notify_all();
	for (auto& t : threads)
		t.join();
}

void ThreadPool::Submit(std::function<void()> task)
{
	{
		std::lock_guard lock(mutex);
		tasks.push_back(std::move(task));
	}
	cv.notify_one();
}

void ThreadPool::Run(int priority)
{
	SetThreadPriority(GetCurrentThread(), priority);
	for (;;)
	{
		std::function<void()> task;
		{
			std::unique_lock lock(mutex);
			cv.wait(lock, [this] { return stopping || !tasks.empty(); });
			if (tasks.empty())
				return;	// stopping and nothing left
			task = std::move(tasks.front());
			tasks.pop_front();
		}
		task();
	}
}

void TaskGroup::Add(int n)
{
	std::lock_guard lock(mutex);
	pending += n;
}

void TaskGroup::Done()
{
	std::lock_guard lock(mutex);
	if (--pending == 0)
		cv.notify_all();
}

void TaskGroup::Wait()
{
	std::unique_lock lock(mutex);
	cv.wait(lock, [this] { return pending <= 0; });
}

}	// namespace cbx
