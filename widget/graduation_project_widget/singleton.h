#ifndef SINGLETON_H
#define SINGLETON_H

#include <memory>
#include <mutex>

template <typename T>
class Singleton {
protected:
    Singleton() = default;
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>&) = delete;
    virtual ~Singleton() = default;

public:
    static std::shared_ptr<T> GetInstance() {
        static std::once_flag once_flag;
        std::call_once(once_flag, []() {
            instance_ = std::shared_ptr<T>(new T());
        });
        return instance_;
    }

private:
    static std::shared_ptr<T> instance_;
};

template <typename T>
std::shared_ptr<T> Singleton<T>::instance_ = nullptr;

#endif // SINGLETON_H
