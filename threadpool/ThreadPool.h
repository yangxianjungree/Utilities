// -*- C++ -*-
// Copyright (c) 2012-2015 Jakob Progsch
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
//    1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
//
//    2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
//
//    3. This notice may not be removed or altered from any source
//    distribution.
//
// Modified for Stephen, copyright (c) 2014-2015 Václav Zeman.

#ifndef __STEPHEN_THREAD_POOL_H__
#define __STEPHEN_THREAD_POOL_H__

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

namespace Stephen
{

class ThreadPool
{
private:
	/* thread vector and task queue */
	std::vector<std::thread>			mWorkerVector;
	std::queue<std::function<void()>>	mTaskQueue;

	std::mutex							mMutex;			// The main mutex.
	std::condition_variable				mCondition;		// Synchronization.
	bool								mStopped;		// The flag that all threads stop.

	std::mutex							mInFlightMutex;
	std::condition_variable				mInFlightCondition;
	std::atomic<std::size_t>			mInFlight;

public:
	explicit ThreadPool(size_t threads = (std::max)(2u, std::thread::hardware_concurrency() * 2));
	~ThreadPool();

	template<class F, class... Args>
	auto Enqueue(F&& f, Args&&... args)->std::future<typename std::result_of<F(Args...)>::type>;

	inline void WaitUntilTasksEmpty();
	inline void WaitUntilNothingInFlight();

private:
	struct HandleInFlight
	{
		ThreadPool& threadPool;

		HandleInFlight(ThreadPool& tp)
			: threadPool(tp)
		{
			std::atomic_fetch_add_explicit(&threadPool.mInFlight,
								  std::size_t(1),
								  std::memory_order_relaxed);
		}

		~HandleInFlight()
		{
			std::size_t prev = std::atomic_fetch_sub_explicit(&threadPool.mInFlight,
													 std::size_t(1),
													 std::memory_order_consume);
			if (prev == 1) {
				threadPool.mInFlightCondition.notify_all();
			}
		}
	};
};

/****************************************************************************************
 * The constructor
 * 
 * Depending how many worker threads we initialize at first.
*****************************************************************************************/
ThreadPool::ThreadPool(size_t threads)
	: mStopped(false)
	, mInFlight(0)
{
	for (size_t i = 0; i < threads; i++)
	{
		mWorkerVector.emplace_back(
			[this]
			{
				while (true)
				{
					std::function<void()> task;
					bool isEmpty;
					{
						std::unique_lock<std::mutex> lock(this->mMutex);
						this->mCondition.wait(lock, [this] { return this->mStopped || !this->mTaskQueue.empty(); });

						if (this->mStopped && this->mTaskQueue.empty()) {
							return;
						}

						task = std::move(this->mTaskQueue.front());
						this->mTaskQueue.pop();
						isEmpty = this->mTaskQueue.empty();
					}

					if (isEmpty) {
						mCondition.notify_all();
					}

					HandleInFlight(*this);
					task();
				}
			}
		);
	}
}

/****************************************************************************************
 * The destructor
 * 
 * Join all threads.
*****************************************************************************************/
ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(mMutex);
		mStopped = true;
	}

	mCondition.notify_all();

	for (std::thread& worker : mWorkerVector)
	{
		worker.join();
	}
}

/****************************************************************************************
 * Method Enqueue
 *
 * @brief:			Insert the function that the worker thread calls.
 *
 * @param f:		The function that the worker thread calls.
 * @param args:		The parameter(s) that the function calls.
 *
 * @return void
*****************************************************************************************/
template<class F, class... Args>
auto ThreadPool::Enqueue(F&& f, Args&&... args)->std::future<typename std::result_of<F(Args...)>::type>
{
	using returnType = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<returnType()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	std::future<returnType> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(mMutex);

		if (mStopped) {
			throw std::runtime_error("Enqueue on stopped ThreadPool.");
		}

		mTaskQueue.emplace([task](){ (*task)(); });
	}

	mCondition.notify_one();

	return res;
}

/****************************************************************************************
 * Method WaitUntilTasksEmpty
 *
 * @brief:			Waiting until the task queue is empty.
 *
 * @return void
*****************************************************************************************/
inline void ThreadPool::WaitUntilTasksEmpty()
{
	std::unique_lock<std::mutex> lock(mMutex);
	mCondition.wait(lock, [this]{ return this->mTaskQueue.empty(); });
}

/****************************************************************************************
 * Method WaitUntilNothingInFlight
 *
 * @brief:			Waiting until nothing in thread pool is in flight.
 *
 * @return void
*****************************************************************************************/
inline void ThreadPool::WaitUntilNothingInFlight()
{
	std::unique_lock<std::mutex> lock(mInFlightMutex);
	mInFlightCondition.wait(lock, [this]{ return (this->mInFlight == 0); });
}

} // end namespace Stephen

#endif//__STEPHEN_THREAD_POOL_H__