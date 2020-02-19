
#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>
#include <map>
#include <unordered_map>
#include <random>
#include <tuple>
#include <sstream>
#include <iostream>
#include <iomanip>

#include "STLTest.h"

using namespace std::chrono;


namespace stltest {

    constexpr uint32_t num_repeats = 200;
    constexpr uint32_t n = 10000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1 << 30);

    template<typename T>
    std::tuple<double, double> insertMap() {

        high_resolution_clock::time_point t1, t2;
        double dmax = 0.0, dur = 0.0;

        T map;

        for (uint32_t j = 0; j < num_repeats; ++j) {
            map.clear();

            t1 = high_resolution_clock::now();
            for (uint32_t i = 0; i < n; ++i) {
                map.emplace(dis(gen), dis(gen));
            }
            t2 = high_resolution_clock::now();

            duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
            double ts = time_span.count();
            dur += ts;
            dmax = ts > dmax ? ts : dmax;
        }

        return { dur / num_repeats, dmax };
    }


    template<typename T>
    std::tuple<double, double> copyMap() {

        high_resolution_clock::time_point t1, t2;
        double dmax = 0.0, dur = 0.0;

        T map;
        T newmap[n];

        for (uint32_t i = 0; i < n; ++i) {
            map.emplace(dis(gen), dis(gen));
        }

        for (uint32_t j = 0; j < num_repeats; ++j) {
            t1 = high_resolution_clock::now();
            newmap[j] = map;
            t2 = high_resolution_clock::now();

            duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
            double ts = time_span.count();
            dur += ts;
            dmax = ts > dmax ? ts : dmax;
        }

        return { dur / num_repeats, dmax };
    }


    template< typename T>
    void runTypedTests(std::string name ) {
       
        auto [dur1, dmax1] = insertMap<T>();
        std::cout << "Emplace into " << std::setw(15) << name << " for " << n << " objects: " << dur1 << " (" << dmax1 << ") seconds. Per object: " << dur1 / n << " seconds." << std::endl;

        auto [dur5, dmax5] = copyMap<T>();
        std::cout << "Copy from    " << std::setw(15) << name << " for " << n << " objects: " << dur5 << " (" << dmax5 << ") seconds. Per object: " << dur5 / n << " seconds." << std::endl;
    }


    void runSTLTests() {

        runTypedTests<std::map<uint64_t, uint32_t>>("map");
        runTypedTests<std::unordered_map<uint64_t, uint32_t>>("hash map");
        runTypedTests<std::multimap<uint64_t, uint32_t>>("multimap");
        runTypedTests<std::unordered_multimap<uint64_t, uint32_t>>("hash multimap");
    }

}

