#include "PCH.h"
#include "ThreadPool.h"
#include "Utils.h"

ThreadPool::ThreadPool(size_t numThreads)
	: m_NumThreads(numThreads)
{
	CreateThreads();
}

ThreadPool::~ThreadPool()
{
	WaitForTasks();
	DestroyThreads();
}

size_t ThreadPool::GetNumTasksQueued() const
{
	std::scoped_lock lock(m_Mutex);
	return m_TaskQueue.size();
}

size_t ThreadPool::GetNumTasksRunning() const
{
	std::scoped_lock lock(m_Mutex);
	return m_NumTasksTotal - m_TaskQueue.size();
}

void ThreadPool::Pause()
{
	m_Paused = true;
}

void ThreadPool::Unpause()
{
	m_Paused = false;
	m_TaskAvailableCV.notify_all();
	m_TaskDoneCV.notify_all();
}

void ThreadPool::Reset()
{
	const bool wasPaused = m_Paused;
	m_Paused = true;
	WaitForTasks();
	DestroyThreads();
	m_Paused = wasPaused;
	CreateThreads();
}

void ThreadPool::WaitForTasks()
{
	m_Waiting = true;
	std::unique_lock lock(m_Mutex);
	m_TaskDoneCV.wait(lock, [this]
		{
			return m_NumTasksTotal == (m_Paused ? m_TaskQueue.size() : 0);
		});
	m_Waiting = false;
}

void ThreadPool::PushBarrier()
{
	{		
		const std::scoped_lock lock(m_Mutex);
		m_TaskQueue.emplace(TaskEntry::Barrier, [this]
		{
			//print("Begin Barrier\n");
			std::unique_lock lock(m_Mutex);
			m_TaskDoneCV.wait(lock, [this]
				{
					// Wait until there are no more running tasks besides this one.
					return m_NumTasksTotal - m_TaskQueue.size() == 1;
				});

			//print("End Barrier\n");
			m_Barrier = false;
			m_TaskAvailableCV.notify_all();
		});
	}
	++m_NumTasksTotal;
	m_TaskAvailableCV.notify_one();
}

void ThreadPool::CreateThreads() {
	m_Running = true;
	m_Threads = new std::thread[m_NumThreads];
	for (size_t i = 0; i < m_NumThreads; ++i) {
		m_Threads[i] = std::thread(&ThreadPool::Worker, this);
		SetThreadName(m_Threads[i], L"Worker Thread " + std::to_wstring(i));
	}	
}

void ThreadPool::DestroyThreads()
{
	m_Running = false;
	m_TaskAvailableCV.notify_all();
	for (size_t i = 0; i < m_NumThreads; ++i)
		m_Threads[i].join();
	delete[] m_Threads;
}

void ThreadPool::Worker()
{
	while(m_Running)
	{
		std::unique_lock lock(m_Mutex);
		m_TaskAvailableCV.wait(lock, [this]
			{
				return !m_TaskQueue.empty() || !m_Running;
			});

		if (m_Running && !m_Paused && !m_Barrier)
		{
			const auto taskEntry = std::move(m_TaskQueue.front());
			m_TaskQueue.pop();

			if (taskEntry.type == TaskEntry::Barrier)
				m_Barrier = true;

			lock.unlock();
			taskEntry.task();
			lock.lock();

			--m_NumTasksTotal;
			m_TaskDoneCV.notify_all();
		}
	}
}