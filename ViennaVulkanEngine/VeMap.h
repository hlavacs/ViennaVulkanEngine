#pragma once


template <typename T, int... Is>
auto makeKey(T& t) {
    return std::tuple(std::get<Is>(t) ...);
}


template <typename T, int... Is>
struct typed_map {

    using type = std::tuple<typename std::tuple_element<Is, T>::type...>;

    std::map<type, int> m;
    typed_map() : m() {};

    void mapValue(T& t, int val) {
        auto key = makeKey<T, Is...>(t);
        m[key] = val;
    };

};


