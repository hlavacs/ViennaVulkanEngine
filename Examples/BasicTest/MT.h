#pragma once

#include <type_traits>

#define _NODISCARD [[nodiscard]]
//#include <experimental/generator>

#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <cppcoro/static_thread_pool.hpp>
#include <cppcoro/when_all.hpp>


namespace MT {

    //using namespace vve;
    /*
    using namespace std::experimental;

    generator<double> fibonacci(const double ceiling) {
        double i = 0;
        while (i <= ceiling) {
            co_yield i;
            i++;
        } 
    }

    int testFib() {

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

    void g1(typename decltype(fibonacci(1))::iterator& iter) {
        iter++;
    }


    int testFib2() {

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
    */

    int test() {

        //testFib2();
        return 0;
    }


}


