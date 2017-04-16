#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <thread>
#include <list>
#include <queue>
#include <memory>
#include <condition_variable>

class ThreadPool
{
	std::list<std::thread> _threads;
	std::queue<std::function<void()>, std::list<std::function<void()> > > _tasks;
	std::shared_ptr<bool> _running;
	
	std::mutex _mutex;
	std::condition_variable _conditionVariable;
	
	static void threadLoop(
		std::shared_ptr<bool> running,
		std::queue<std::function<void()>, std::list<std::function<void()> > >& tasks,
		std::mutex& mutex,
		std::condition_variable& conditionVariable)
	{
		while(*running)
		{
			std::unique_lock<std::mutex> lock(mutex);
			
			if(!*running)
			{
				return;
			}
			
			if(tasks.size() > 0)
			{
				auto task = tasks.front();
				tasks.pop();
				lock.unlock();
				task();
				
				continue;
			}
			
			conditionVariable.wait(lock);
			
			if(!*running)
			{
				return;
			}
			
			if(tasks.size() > 0)
			{
				auto task = tasks.front();
				tasks.pop();
				lock.unlock();
				task();
				
				continue;
			}
		}
	}
	
public:

	ThreadPool(size_t threadCount)
		: _running(new bool(true))
	{
		for(size_t i=0; i<threadCount; i++)
		{
			_threads.emplace_back(
				std::thread(
					threadLoop,
					_running,
					std::ref(_tasks),
					std::ref(_mutex),
					std::ref(_conditionVariable)
				)
			);
		}
	}
	
	~ThreadPool()
	{
		*_running = false;
		
		std::unique_lock<std::mutex> lock(_mutex);
		
		_conditionVariable.notify_all();
		lock.unlock();
		
		for(auto& thread : _threads)
		{
			if(thread.joinable())
			{
				thread.join();
			}
		}
	}
	
	template<typename ...Args>
	void addTask(void(*rawTask)(Args...), Args... args)
	{
		auto task = std::bind(rawTask, args...);
		std::unique_lock<std::mutex> lock(_mutex);
		
		_tasks.emplace(task);
		_conditionVariable.notify_one();
	}
	
	void clearTasks()
	{
		std::unique_lock<std::mutex> lock(_mutex);
		
		_tasks = std::queue<std::function<void()>, std::list<std::function<void()> > >();
	}
	
	void detachThreads()
	{
		for(auto& thread : _threads)
		{
			if(thread.joinable())
			{
				thread.detach();
			}
		}
	}
	
	int getTaskCount()
	{
		std::unique_lock<std::mutex> lock(_mutex);
		
		return _tasks.size();
	}
};

#endif