#pragma once

#include <type_traits>

#define _NODISCARD [[nodiscard]]
#include <experimental/coroutine>
#include <experimental/generator>

#include <future>
#include <iostream>

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
        co_await suspend_never{};
        std::cout << "World" << std::endl;
    }



    inline int test() {

        testFib2();
        return 0;
    }


}


