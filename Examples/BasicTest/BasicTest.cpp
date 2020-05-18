#include <iostream>
#include <functional>
#include <tuple>
#include <array>
#include <map>
#include <type_traits>
#include <utility>


#include "VEInclude.h"


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


template <typename T, int... Is>
auto makeKey(T& t) {
    return std::tuple( std::get<Is>(t) ...);
}


template <typename T, int... Is>
struct typed_map {

    using type = std::tuple<typename std::tuple_element<Is, T>::type...>;

    std::map<type,int> m;
    typed_map() : m() {};

    void mapValue( T& t, int val) {
        auto key = makeKey<T, Is...>(t);
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

    table_info k3 = { 4.0f, "C", 6 };
    typed_map<table_info, 2, 1 > map2;
    typed_map<table_info, 0, 1, 2 > map3;

    map2.mapValue(k3, 5);
    map3.mapValue(k3, 6);

    std::cout << sizeof std::tuple<VeIndex64> << "\n";

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

    return 0;
}

