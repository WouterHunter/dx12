#pragma once

template<class T>
class ThreadSafeQueue
{
public:
	ThreadSafeQueue() = default;
	~ThreadSafeQueue() = default;

	void Push(const T& value)
	{
		std::lock_guard lock(m_Mutex);
		m_DataQueue.push(value);
		m_CV.notify_one();
	}

	void Push(T&& value)
	{
		std::lock_guard lock(m_Mutex);
		m_DataQueue.push(std::forward<T>(value));
		m_CV.notify_one();
	}

	template<class...Args>
	T& Emplace(Args&&... args)
	{
		std::lock_guard lock(m_Mutex);
		T& result = m_DataQueue.emplace(std::forward<Args>(args)...);
		m_CV.notify_one();
		return result;
	}

	void WaitAndPop(T& value)
	{
		std::unique_lock lock(m_Mutex);
		m_CV.wait(lock, [this] { return !m_DataQueue.empty(); });
		value = m_DataQueue.front();
		m_DataQueue.pop();
	}

	bool TryPop(T& value)
	{
		std::unique_lock lock(m_Mutex);
		if (m_DataQueue.empty()) return false;
		value = m_DataQueue.front();
		m_DataQueue.pop();
		return true;
	}

	[[nodiscard]] bool Empty() const
	{
		return m_DataQueue.empty();
	}

	[[nodiscard]] size_t Size() const
	{
		return m_DataQueue.size();
	}

	// USE WITH CARE: this function is not threadsafe.
	[[nodiscard]] std::queue<T>& GetData()
	{
		return m_DataQueue;
	}

private:
	mutable std::mutex m_Mutex;
	std::queue<T> m_DataQueue{};
	std::condition_variable m_CV;
};