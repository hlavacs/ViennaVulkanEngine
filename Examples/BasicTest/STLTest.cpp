
#include <iostream>
#include <ctime>
#include <ratio>
#include <chrono>
#include <map>
#include <unordered_map>
#include <random>
#include <tuple>

#include "STLTest.h"

using namespace std::chrono;


namespace stltest {

    constexpr uint32_t num_repeats = 200;
    constexpr uint32_t n = 1000;
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



    void runTests() {
       
        auto [dur1, dmax1] = insertMap<std::map<uint64_t, uint32_t>>();
        std::cout << "Emplace into           map for " << n << " objects: " << dur1 << " (" << dmax1 << ") seconds. Per object: " << dur1 / n << " seconds." << std::endl;

        auto [dur2, dmax2] = insertMap<std::unordered_map<uint64_t, uint32_t>>();
        std::cout << "Emplace into      hash map for " << n << " objects: " << dur2 << " (" << dmax2 << ") seconds. Per object: " << dur2 / n << " seconds." << std::endl;

        auto [dur3, dmax3] = insertMap<std::multimap<uint64_t, uint32_t>>();
        std::cout << "Emplace into      multimap for " << n << " objects: " << dur3 << " (" << dmax3 << ") seconds. Per object: " << dur3 / n << " seconds." << std::endl;

        auto [dur4, dmax4] = insertMap<std::unordered_multimap<uint64_t, uint32_t>>();
        std::cout << "Emplace into hash multimap for " << n << " objects: " << dur4 << " (" << dmax4 << ") seconds. Per object: " << dur4 / n << " seconds." << std::endl;

        //----------

        auto [dur5, dmax5] = copyMap<std::map<uint64_t, uint32_t>>();
        std::cout << "Copy           map for " << n << " objects: " << dur5 << " (" << dmax5 << ") seconds. Per object: " << dur5 / n << " seconds." << std::endl;

        auto [dur6, dmax6] = copyMap<std::unordered_map<uint64_t, uint32_t>>();
        std::cout << "Copy      hash map for " << n << " objects: " << dur6 << " (" << dmax6 << ") seconds. Per object: " << dur6 / n << " seconds." << std::endl;

        auto [dur7, dmax7] = copyMap<std::multimap<uint64_t, uint32_t>>();
        std::cout << "Copy      multimap for " << n << " objects: " << dur7 << " (" << dmax7 << ") seconds. Per object: " << dur7 / n << " seconds." << std::endl;

        auto [dur8, dmax8] = copyMap<std::unordered_multimap<uint64_t, uint32_t>>();
        std::cout << "Copy hash multimap for " << n << " objects: " << dur8 << " (" << dmax8 << ") seconds. Per object: " << dur8 / n << " seconds." << std::endl;


    }


}

