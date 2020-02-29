
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

#include "VEInclude.h"
#include "VEVector.h"
#include "STLTest.h"

using namespace std::chrono;


namespace stltest {

    constexpr uint32_t num_repeats = 700;
    constexpr uint32_t n = 15000;
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
            dmax = std::max(ts, dmax);
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
            dmax = std::max(ts, dmax);
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



    struct VectorTestStruct {
        uint32_t i32;
        uint64_t i64;
        uint64_t a[5];
        VectorTestStruct() : i32(0), i64(0) {};
        VectorTestStruct(uint32_t v32, uint64_t v64) : i32(v32), i64(v64), a{0,0,0,0,0} {};
        //void operator=(const VectorTestStruct& src) {
        //    memcpy(this, &src, sizeof(VectorTestStruct));
        //};
    };



    template<typename T>
    std::tuple<double, double> insertVeVector( bool memcopy ) {

        high_resolution_clock::time_point t1, t2;
        double dmax = 0.0, dur = 0.0;

        T vec(memcopy);
        vec.reserve(n);

        for (uint32_t j = 0; j < num_repeats; ++j) {
            vec.clear();

            t1 = high_resolution_clock::now();
            for (uint32_t i = 0; i < n; ++i) {
                vec.push_back({ (uint32_t)dis(gen), (uint64_t)dis(gen) });
            }
            t2 = high_resolution_clock::now();

            duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
            double ts = time_span.count();
            dur += ts;
            dmax = std::max(ts, dmax);
        }

        return { dur / num_repeats, dmax };
    }


    template<typename T>
    std::tuple<double, double> copyVeVector( bool memcopy) {

        high_resolution_clock::time_point t1, t2;
        double dmax = 0.0, dur = 0.0;

        T vec(memcopy);
        vec.reserve(n);

        T newvec(memcopy);
        newvec.reserve(n);

        for (uint32_t i = 0; i < n; ++i) {
            vec.push_back({ (uint32_t) dis(gen), (uint64_t)dis(gen) } );
        }

        for (uint32_t j = 0; j < num_repeats; ++j) {
            t1 = high_resolution_clock::now();
            newvec = vec;
            t2 = high_resolution_clock::now();

            duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
            double ts = time_span.count();
            dur += ts;
            dmax = std::max( ts, dmax );
        }

        return { dur / num_repeats, dmax };
    }


    template<typename T>
    void runVeVectorTests(std::string name, bool memcopy) {
        auto [dur1, dmax1] = insertVeVector<T>(memcopy);
        std::cout << "Emplace into " << std::setw(15) << name << " for " << n << " objects: " << dur1 << " (" << dmax1 << ") seconds. Per object: " << dur1 / n << " seconds." << std::endl;

        auto [dur5, dmax5] = copyVeVector<T>(memcopy);
        std::cout << "Copy from    " << std::setw(15) << name << " for " << n << " objects: " << dur5 << " (" << dmax5 << ") seconds. Per object: " << dur5 / n << " seconds." << std::endl;
    }


    template<typename T>
    std::tuple<double, double> insertVector() {

        high_resolution_clock::time_point t1, t2;
        double dmax = 0.0, dur = 0.0;

        T vec;
        vec.reserve(n);

        for (uint32_t j = 0; j < num_repeats; ++j) {
            vec.clear();

            t1 = high_resolution_clock::now();
            for (uint32_t i = 0; i < n; ++i) {
                vec.emplace_back( (uint32_t)dis(gen), (uint64_t)dis(gen) );
            }
            t2 = high_resolution_clock::now();

            duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
            double ts = time_span.count();
            dur += ts;
            dmax = std::max(ts, dmax);
        }

        return { dur / num_repeats, dmax };
    }


    template<typename T>
    std::tuple<double, double> copyVector() {

        high_resolution_clock::time_point t1, t2;
        double dmax = 0.0, dur = 0.0;

        T vec;
        vec.reserve(n);

        T newvec;
        newvec.reserve(n);

        for (uint32_t i = 0; i < n; ++i) {
            vec.emplace_back( (uint32_t)dis(gen), (uint64_t)dis(gen) );
        }

        for (uint32_t j = 0; j < num_repeats; ++j) {
            t1 = high_resolution_clock::now();
            newvec = vec;
            t2 = high_resolution_clock::now();

            duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
            double ts = time_span.count();
            dur += ts;
            dmax = std::max(ts, dmax);
        }

        return { dur / num_repeats, dmax };
    }


    template<typename T>
    void runVectorTests(std::string name) {
        auto [dur1, dmax1] = insertVector<T>();
        std::cout << "Emplace into " << std::setw(15) << name << " for " << n << " objects: " << dur1 << " (" << dmax1 << ") seconds. Per object: " << dur1 / n << " seconds." << std::endl;

        auto [dur5, dmax5] = copyVector<T>();
        std::cout << "Copy from    " << std::setw(15) << name << " for " << n << " objects: " << dur5 << " (" << dmax5 << ") seconds. Per object: " << dur5 / n << " seconds." << std::endl;
    }


    void runSTLTests() {

        runVeVectorTests<vve::VeVector<VectorTestStruct>>("VeVector(false)", false );
        runVeVectorTests<vve::VeVector<VectorTestStruct>>("VeVector(true)", true);

        runVectorTests<std::vector<VectorTestStruct>>("std::vector");

        return;
        runTypedTests<std::map<uint64_t, uint32_t>>("map");
        runTypedTests<std::unordered_map<uint64_t, uint32_t>>("hash map");
        runTypedTests<std::multimap<uint64_t, uint32_t>>("multimap");
        runTypedTests<std::unordered_multimap<uint64_t, uint32_t>>("hash multimap");
    }

}

