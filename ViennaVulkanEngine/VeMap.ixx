export module VVE:VeMap;

import std.core;
import :VeTypes;


export namespace vve { 
    template <typename T, int... Is>
    auto makeKey(T& t) {
        return std::tuple(std::get<Is>(t) ...);
    }

    template <typename T, int... Is>
    struct typed_map {

        std::map<T, int> m;
        typed_map() : m() {};

        void mapValue(T& t, int val) {
            auto key = makeKey<T, Is...>(t);
            m[key] = val;
        };
    };


    struct VeMapBase {
    };

    template<int... Is>
    struct VeMap : VeMapBase {
        static constexpr auto s_indices = std::make_tuple(Is...);
        static const uint32_t initial_size = 64;
        std::vector<VeIndex16> d_array;
        VeMap<Is...>() : d_array(initial_size, VeIndex16(0) ) {}
    };




};





