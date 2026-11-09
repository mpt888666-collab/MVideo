//
// Created by mpt on 2026/10/15.
//

#ifndef GETSERVER_SINGLETON_H
#define GETSERVER_SINGLETON_H
#include <memory>
#include <iostream>

template <typename T>
class Singleton {
protected:
    Singleton() = default;

    Singleton(const Singleton&) = delete;

    Singleton(Singleton&&) = delete;

    Singleton& operator=(const Singleton&) = delete;

    Singleton& operator=(Singleton&&) = delete;

public:
    static std::shared_ptr<T> GetInstance() {
        static std::shared_ptr<T> instance = std::shared_ptr<T>(new T());
        return instance;
    }
};
#endif  // GETSERVER_SINGLETON_H
