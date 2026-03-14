#pragma once

#include <boost/asio.hpp>

#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace edge::net {

// IO 线程池：管理多个 io_context，并以轮询方式分配给新会话。
class IOContextPool {
public:
    explicit IOContextPool(std::size_t size = 4);
    ~IOContextPool();

    IOContextPool(const IOContextPool&) = delete;
    IOContextPool& operator=(const IOContextPool&) = delete;

    // 启动所有 io_context 的工作线程。
    void Start();
    // 停止所有 io_context 并回收线程。
    void Stop();

    // 轮询获取一个 io_context，用于绑定新连接会话。
    boost::asio::io_context& GetIOContext();

private:
    using WorkGuard = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

    std::vector<std::shared_ptr<boost::asio::io_context>> io_contexts_;
    std::vector<std::shared_ptr<WorkGuard>> works_;
    std::vector<std::thread> threads_;
    std::size_t next_index_ = 0;
};

}  // namespace edge::net
