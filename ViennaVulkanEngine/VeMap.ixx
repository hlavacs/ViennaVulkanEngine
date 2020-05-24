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
        using type = std::tuple<decltype(Is)...>;
            
        type d_data = std::make_tuple(Is...);
    };


    // A template to hold a parameter pack
    template < typename... > 
    struct Typelist {};

    // Declaration of a template
    template< typename... Types> struct VeMapTable;

    template< typename... TypesOne, typename... TypesTwo>
    struct VeMapTable< Typelist < TypesOne... >, Typelist < TypesTwo... > > {

        using TupleTypeOne = std::tuple< TypesOne... >;
        using TupleTypeTwo = std::tuple< TypesTwo... >;

        TupleTypeOne d_one;
        TupleTypeTwo d_two;

    };


};





