#pragma once
#include <future>
#include <type_traits>
#include <queue>

class ThreadPool
{
public:
	explicit ThreadPool(size_t numThreads = 0);
	~ThreadPool();

	[[nodiscard]] size_t GetNumTasksQueued() const;
	[[nodiscard]] size_t GetNumTasksRunning() const;
	[[nodiscard]] size_t GetNumTasksTotal() const { return m_NumTasksTotal; }
	[[nodiscard]] size_t GetNumThreads() const { return m_NumThreads; }
	[[nodiscard]] bool IsPaused() const { return m_Paused; }

	void Pause();
	void Unpause();
	void Reset();
	void WaitForTasks();
	void PushBarrier();
	
	template <class F, class... A>
	void PushTask(F&& task, A&&... args);
	
	template <class F, class... A, class R = std::invoke_result_t<std::decay_t<F>, std::decay_t<A>...>>
	[[nodiscard]] std::future<R> Submit(F&& task, A&&... args);

private:

	// Private Methods -----------------------------------------------------------------
	
	void CreateThreads();
	void DestroyThreads();
	void Worker();

	// Private Members -----------------------------------------------------------------

	std::atomic<bool> m_Running{ false };
	std::atomic<bool> m_Paused{ false };
	std::atomic<bool> m_Waiting{ false };
	std::atomic<bool> m_Barrier{ false };

	std::condition_variable m_TaskAvailableCV{};
	std::condition_variable m_TaskDoneCV{};

	std::atomic<size_t> m_NumTasksTotal{ 0 };

	struct TaskEntry
	{
		enum TaskType { Task, Barrier } type{ Task };
		std::function<void()> task;
	};
	std::queue<TaskEntry> m_TaskQueue{};

	size_t m_NumThreads{ 0 };
	std::thread* m_Threads{ nullptr };

	mutable std::mutex m_Mutex{};
};

template <class F, class ... A>
void ThreadPool::PushTask(F&& task, A&&... args)
{
	{
		std::function<void()> taskFunction = std::bind(std::forward<F>(task), std::forward<A>(args)...);
		const std::scoped_lock lock(m_Mutex);
		m_TaskQueue.emplace(TaskEntry::Task, taskFunction);
	}
	++m_NumTasksTotal;
	m_TaskAvailableCV.notify_one();
}

template <class F, class ... A, class R>
std::future<R> ThreadPool::Submit(F&& task, A&&... args)
{
	//std::function<R> taskFunction = [t = std::forward<F>(task), ... a = std::forward<A>(args)] { return std::invoke(t, a...); };
	std::function<R> taskFunction = std::bind(std::forward<F>(task), std::forward<A>(args)...);
	std::shared_ptr<std::promise<R>> taskPromise = std::make_shared<std::promise<R>>();
	PushTask([taskFunction, taskPromise]
		{
			try
			{
				if constexpr (std::is_void_v<R>)
				{
					std::invoke(taskFunction);
					taskPromise->set_value();
				}
				else
				{
					taskPromise->set_value(std::invoke(taskFunction));
				}
			}
			catch(...)
			{
				try
				{
					taskPromise->set_exception(std::current_exception());
				}
				catch(...) {}
			}
		});
	return taskPromise->get_future();
}
