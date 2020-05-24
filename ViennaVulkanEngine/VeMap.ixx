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
    };


    template<typename T>
    struct VeMap_impl {
        //static type = std::tuple<T>;
    };

    template<typename T, typename... Args>
    struct VeMap_impl1 {
       // if conditional is_same_v

        //static type = std::tuple<T, VeMap_impl<Args>::type>;

    };

};





