#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ThreadPoolQueue {
public:
    ThreadPoolQueue() : m_is_running(true) {};

    void startAsync(unsigned int max_threads)
    {
        for (int i = 0; i < max_threads; ++i)
        {
            m_threads_vec.emplace_back([this]{
                while (m_is_running.load())
                {
                    std::function<void()> task;
                    {
                        std::unique_lock lock(m_mutex);
                        m_cv.wait(lock, [this] { 
                            return !m_is_running.load() || !m_tasks_queue.empty(); 
                        });

                        if (m_tasks_queue.empty())
                        {
                            continue;
                        }
                        task = std::move(m_tasks_queue.front());
                        m_tasks_queue.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPoolQueue() 
    {
        Stop();
    }

    void Stop()
    {
        m_is_running.store(false);
        m_cv.notify_all();
        for (auto& task : m_threads_vec)
        {
            if (task.joinable())
            {
                task.join();
            }
        }
    }

    template<class T>
    void Push(T&& task) 
    {
        if (!m_is_running.load())
        {
            return;
        }
        {
            std::unique_lock lock(m_mutex);
            m_tasks_queue.emplace(std::forward<T>(task));
        }
        m_cv.notify_one();
    }
private:
    std::vector<std::thread> m_threads_vec;
    std::queue<std::function<void()>> m_tasks_queue;
    std::mutex m_mutex;
    std::atomic<bool> m_is_running;
    std::condition_variable m_cv;
};