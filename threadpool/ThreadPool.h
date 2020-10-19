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
	std::vector<std::thread>			mWorkers;
	std::queue<std::function<void()>>	mTasks;
	/* synchronizatin */
	std::mutex							mQueueMutex;
	std::condition_variable				mCondition;
	bool								mStopped;

public:
	ThreadPool(size_t);
	~ThreadPool();

	template<class F, class... Args>
	auto Enqueque(F&& f, Args&&... args)->std::future<typename std::result_of<F(Args...)>::type>;
};

/****************************************************************************************
 * The constructor
 * 
 * Depending how many worker threads we initilized.
*****************************************************************************************/
ThreadPool::ThreadPool(size_t threads)
	: mStopped(false)
{
	for (size_t i = 0; i < threads; i++)
	{
		mWorkers.emplace_back(
			[this]
			{
				while (true)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(this->mQueueMutex);
						this->mCondition.wait(lock, [this] {
							return this->mStopped || !this->mTasks.empty();
						});
						
						if (this->mStopped && this->mTasks.empty())
						{
							return;
						}

						task = std::move(this->mTasks.front());
						this->mTasks.pop();
					}

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
		std::unique_lock<std::mutex> lock(mQueueMutex);
		mStopped = true;
	}

	mCondition.notify_all();

	for (std::thread& worker : mWorkers)
	{
		worker.join();
	}
}

template<class F, class... Args>
auto ThreadPool::Enqueque(F&& f, Args&&... args)->std::future<typename std::result_of<F(Args...)>::type>
{
	using returnType = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared<std::packaged_task<returnType()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	std::future<returnType> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(mQueueMutex);

		if (mStopped)
		{
			throw std::runtime_error("Enqueue on stopped ThreadPool.");
		}

		mTasks.emplace([task](){ (*task)(); });
	}

	mCondition.notify_one();

	return res;
}

}

#endif//__STEPHEN_THREAD_POOL_H__