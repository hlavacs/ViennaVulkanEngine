#include <iostream>
#include <functional>
#include <tuple>
#include <array>
#include <map>

#include "VEInclude.h"
#include "jobSystemTest.h"
#include "STLTest.h"

#include <memory_resource>
#include <experimental/coroutine>
#include <filesystem>
#include <experimental/generator>

using namespace vve;
using namespace std;
using namespace std::experimental;

template<typename T>
concept EqualityComparable = requires(T a, T b) {
    { a == b }->std::boolean;
    { a != b }->std::boolean;
};

generator<int> generatorForNumbers(int begin, int inc = 1) {

    for (int i = begin;; i += inc) {
        co_yield i;
    }

}

namespace detail {
    template <typename T> class empty_base {};
}

template <class T, class U, class B = ::detail::empty_base<T> >
struct less_than_comparable2 : B
{
    friend bool operator<=(const T& x, const U& y) { return !(x > y); }
    friend bool operator>=(const T& x, const U& y) { return !(x < y); }
    friend bool operator>(const U& x, const T& y) { return y < x; }
    friend bool operator<(const U& x, const T& y) { return y > x; }
    friend bool operator<=(const U& x, const T& y) { return !(y < x); }
    friend bool operator>=(const U& x, const T& y) { return !(y > x); }
};

template <class T, class B = ::detail::empty_base<T> >
struct less_than_comparable1 : B
{
    friend bool operator>(const T& x, const T& y) { return y < x; }
    friend bool operator<=(const T& x, const T& y) { return !(y < x); }
    friend bool operator>=(const T& x, const T& y) { return !(x < y); }
};

template <class T, class U, class B = ::detail::empty_base<T> >
struct equality_comparable2 : B
{
    friend bool operator==(const U& y, const T& x) { return x == y; }
    friend bool operator!=(const U& y, const T& x) { return !(x == y); }
    friend bool operator!=(const T& y, const U& x) { return !(y == x); }
};

template <class T, class B = ::detail::empty_base<T> >
struct equality_comparable1 : B
{
    friend bool operator!=(const T& x, const T& y) { return !(x == y); }
};

template <class T, class U, class B = ::detail::empty_base<T> >
struct totally_ordered2 : less_than_comparable2<T, U, equality_comparable2<T, U, B> > {};

template <class T, class B = ::detail::empty_base<T> >
struct totally_ordered1 : less_than_comparable1<T, equality_comparable1<T, B> > {};


#define SAFE_TYPEDEF(T, D)                                      \
struct D : totally_ordered1< D, totally_ordered2< D, T > >      \
{                                                               \
    T t;                                                        \
    explicit D(const T& t_) : t(t_) {};                         \
    explicit D(T&& t_) : t(std::move(t_)) {};                   \
    D() = default;                                              \
    D(const D & t_) = default;                                  \
    D(D&&) = default;                                           \
    D& operator=(const D & rhs) = default;                     \
    D& operator=(D&&) = default;                               \
    operator T& () { return t; }                               \
    bool operator==(const D & rhs) const { return t == rhs.t; } \
    bool operator<(const D & rhs) const { return t < rhs.t; }   \
};


SAFE_TYPEDEF(std::uint16_t, VeIndex16);
SAFE_TYPEDEF(std::uint32_t, VeIndex32);
SAFE_TYPEDEF(std::uint64_t, VeIndex64);
SAFE_TYPEDEF(std::uint32_t, VeGuid32);
SAFE_TYPEDEF(std::uint64_t, VeGuid64);

typedef std::tuple<VeIndex16> VeIndexTuple;



auto tfun(VeIndex16 arg) {
    int a[10];
    a[arg + 1] = 5;
    return arg;
}

template < typename T, int idx = 0>
struct getter {
    auto getKey(T& t) {
        return std::get<idx>(t);
    }

    using U = typename std::tuple_element<idx, T>::type;

    void setKey(T& t, U&& u) {
        get<idx>(t) = u;
    }
};



template <typename T, typename U, typename K>
struct typed_map {
    U mygetter;
    std::map<K, int> m;

    typed_map() : mygetter(), m() {};
    void mapValue( T& t, int val) {
        K key = { std::get<0>(mygetter).getKey(t), std::get<1>(mygetter).getKey(t) };
        m[key] = val; 
    };

};


using table_info = std::tuple<float, std::string, int>;



int main()
{
    std::pmr::vector<int> t;

    VeIndex16 idx(17);
    uint16_t i = idx;

    tfun(idx);


    std::tuple<int> k1 = { 1 };
    std::tuple<int, std::string> k2 = { 2, "A"};

    getter<std::tuple<int>,0> g0;
    getter<std::tuple<int, std::string>,1> g1;
 
    int r = g0.getKey(k1);
    std::string s = g1.getKey(k2);
    g0.setKey(k1, 2);
    g1.setKey(k2, "B");


    table_info k3 = { 4.0f, "C", 6 };
    typed_map<table_info, std::tuple< getter<table_info, 2>, getter<table_info, 1> >,  std::tuple<int, std::string>> map1;

    map1.mapValue(k3, 5);


    std::cout << "Hello World!\n";

    /*JADD( jst::jobSystemTest() );
#ifdef VE_ENABLE_MULTITHREADING
    vgjs::JobSystem::getInstance()->wait();
    vgjs::JobSystem::getInstance()->terminate();
    vgjs::JobSystem::getInstance()->waitForTermination();
#endif
    return 0;*/

    //vec::testVector();
    //map::testMap();
    //tab::testTables();
    //stltest::runSTLTests();
    //return 0;

    syseng::init();
    syseng::runGameLoop();
    syseng::close();
 
    return 0;
}

