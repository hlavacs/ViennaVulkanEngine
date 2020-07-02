#pragma once

#include <type_traits>


#define _NODISCARD [[nodiscard]]
#include <experimental/coroutine>
#include <experimental/resumable>
#include <experimental/generator>

#include <future>
#include <iostream>


namespace std::experimental {

    template <>
    struct coroutine_traits<void> {
        struct promise_type {
            using coro_handle = std::experimental::coroutine_handle<promise_type>;
            auto get_return_object() {
                return coro_handle::from_promise(*this);
            }
            auto initial_suspend() { return std::experimental::suspend_always(); }
            auto final_suspend() { return std::experimental::suspend_always(); }
            void return_void() {}
            void unhandled_exception() {
                std::terminate();
            }
        };
    };

};

namespace MT {

    using namespace vve;
    
    using namespace std::experimental;

    inline generator<double> fibonacci(const double ceiling) {
        double i = 0;
        while (i <= ceiling) {
            co_yield i;
            i++;
        } 
    }

    inline int testFib() {

        const double demo_ceiling = 10;

        auto f = fibonacci(demo_ceiling);
        auto iter = f.begin();
        auto end = f.end();

        while (iter != end) {
            const auto value = *iter;
            std::cout << value << '\n';
            iter++;
        }
        return 0;
    }

    inline void g1(typename decltype(fibonacci(1))::iterator& iter) {
        iter++;
    }


    inline int testFib2() {

        const double demo_ceiling = 10;

        auto f = fibonacci(demo_ceiling);
        auto iter = f.begin();
        auto end = f.end();

        auto g = [&]() {
            while (iter != end) {
                const auto value = *iter;
                std::cout << value << '\n';
                g1( iter );
            };
        };
        g();
        return 0;
    }



    
    inline void foo() {
        std::cout << "Hello" << std::endl;
        co_await suspend_always{};
        std::cout << "World" << std::endl;
    }



    inline int test() {

        //testFib2();
        foo();
        return 0;
    }


}


