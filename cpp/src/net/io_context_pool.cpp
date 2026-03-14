#include "net/io_context_pool.h"

#include <algorithm>
#include <stdexcept>

namespace edge::net {

IOContextPool::IOContextPool(std::size_t size) {
    if (size == 0) {
        throw std::invalid_argument("IOContextPool size must be greater than 0");
    }

    io_contexts_.reserve(size);
    works_.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        auto io = std::make_shared<boost::asio::io_context>();
        // work_guard 用于防止 io_context 在无任务时提前退出 run()。
        auto work = std::make_shared<WorkGuard>(boost::asio::make_work_guard(*io));
        io_contexts_.push_back(io);
        works_.push_back(work);
    }
}

IOContextPool::~IOContextPool() {
    Stop();
}

void IOContextPool::Start() {
    if (!threads_.empty()) {
        return;
    }

    threads_.reserve(io_contexts_.size());
    for (const auto& io : io_contexts_) {
        threads_.emplace_back([io]() {
            io->run();
        });
    }
}

void IOContextPool::Stop() {
    // 先移除保活，再 stop，确保 run() 正常退出。
    for (auto& work : works_) {
        if (work) {
            work->reset();
        }
    }

    for (auto& io : io_contexts_) {
        if (io) {
            io->stop();
        }
    }

    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads_.clear();
}

boost::asio::io_context& IOContextPool::GetIOContext() {
    auto& io = *io_contexts_[next_index_];
    next_index_ = (next_index_ + 1) % io_contexts_.size();
    return io;
}

}  // namespace edge::net
