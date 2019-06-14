#pragma once
#include <vector>
#include <thread>

struct thread_pool
{
	std::vector<std::thread> threads;
	size_t num_threads;

	void resize(const size_t& num_threads)
	{
		this->num_threads = num_threads;
		threads.resize(num_threads);
	};

	 template <class _Fn, class... _Args>
	void add_job(const size_t& thread_id, _Fn&& job, _Args&&... args)
	{
		threads[thread_id] = std::thread(job, args...);
	}

	void wait_for_thread(const size_t& thread_id)
	{
		if (thread_id >= num_threads)
			return;

		if (threads[thread_id].joinable())
			threads[thread_id].join();
	}

	void wait_for_threads()
	{
		for (size_t i = 0; i < num_threads; ++i)
			if (threads[i].joinable())
				threads[i].join();
	}

	void release()
	{
	}

};