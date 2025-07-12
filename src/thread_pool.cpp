#include "fast_pdf_parser/thread_pool.h"

namespace fast_pdf_parser {

ThreadPool::ThreadPool(size_t num_threads) {
    for(size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            for(;;) {
                std::function<void()> task;
                
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this]{ 
                        return this->stop || !this->tasks.empty(); 
                    });
                    
                    if(this->stop && this->tasks.empty()) {
                        return;
                    }
                    
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                
                task();
                
                active_tasks--;
                finished.notify_all();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    
    condition.notify_all();
    
    for(std::thread &worker: workers) {
        worker.join();
    }
}

void ThreadPool::wait_all() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    finished.wait(lock, [this]{ 
        return tasks.empty() && active_tasks == 0; 
    });
}

size_t ThreadPool::queue_size() const {
    std::unique_lock<std::mutex> lock(queue_mutex);
    return tasks.size();
}

size_t ThreadPool::active_threads() const {
    return active_tasks.load();
}

} // namespace fast_pdf_parser