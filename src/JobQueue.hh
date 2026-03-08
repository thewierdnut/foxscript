// foxscript
// Copyright (C) 2026 thewierdnut
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

class JobQueue final
{
public:
   JobQueue(const std::string& name = "job", int thread_count = -1):
      m_queue_name(name)
   {
      if (thread_count == -1)
      {
         thread_count = std::thread::hardware_concurrency() - 2;
         if (thread_count <= 0)
            thread_count = 1;
      }

      m_busy = 0;
      for (size_t i = 0; i < thread_count; ++i)
         m_threads.push_back(std::thread(&JobQueue::Thread, this, i));
   }

   ~JobQueue()
   {
      std::unique_lock<std::mutex> lock(m_work_mutex);
      m_running = false;
      m_work_cv.notify_all();
      lock.unlock();
      for (auto& thread: m_threads)
         thread.join();
   }

   void AddJob(std::function<void()> job)
   {
      std::unique_lock<std::mutex> lock(m_work_mutex);
      m_work.emplace_back(job);
      m_work_cv.notify_all();
   }

   void Wait()
   {
      std::unique_lock<std::mutex> lock(m_work_mutex);
      if (m_threads.empty())
      {
         // Don't assume this is the same thread that called AddJob.
         m_busy = 1;
         for (auto& w: m_work)
            w();
         m_work.clear();
         m_busy = 0;
      }
      else
      {
         m_wait_cv.wait(lock, [this]() { return m_work.empty() && m_busy == 0;} );
      }
   }

protected:
   void Thread(size_t id)
   {
      pthread_setname_np(pthread_self(), (m_queue_name + std::to_string(id)).c_str());
      
      
      while (m_running)
      {
         std::unique_lock<std::mutex> lock(m_work_mutex);
         m_work_cv.wait(lock, [this]() { return !m_running || !m_work.empty(); });
         if (!m_work.empty())
         {
            ++m_busy;
            auto work = m_work.back();
            m_work.pop_back();
            lock.unlock();
            work();

            lock.lock();
            --m_busy;
            m_wait_cv.notify_one();
         }
      }
   }

private:
   const std::string m_queue_name;
   std::vector<std::thread> m_threads;
   std::vector<std::function<void()>> m_work;
   std::condition_variable m_work_cv;
   std::condition_variable m_wait_cv;
   size_t m_busy = 0;
   std::mutex m_work_mutex; // guards m_work and m_busy. Used for both m_work_cv and m_wait_cv

   bool m_running = true;
};
