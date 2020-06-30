#pragma once


#define _NODISCARD [[nodiscard]]
//#include <experimental/coroutine>
#include <experimental/generator>
//#include "generator.h"


namespace MT {

    using namespace vve;
    using namespace std::experimental;

    generator<double> fibonacci(const double ceiling) {
        double j = 0;
        double i = 1;
        co_yield j;
        if (ceiling > j) {
            do {
                co_yield i;
                double tmp = i;
                i += j;
                j = tmp;
            } while (i <= ceiling);
        }
    }

    int testFib() {

        const double demo_ceiling = 10E44;

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


    int test() {

        testFib();
        return 0;
    }


}


