#ifndef VEHASH_H
#define VEHASH_H


//----------------------------------------------------------------------------------
//std::hash specializations

namespace std {

    template<typename... Args>
    struct hash<tuple<Args...>> {
        size_t operator()(const tuple<Args...>& v) {
            return vve::hash_impl(v, make_index_sequence<sizeof...(Args)>());
        }
    };

    template<typename T, typename P>
    struct hash<vve::NumType<T, P>> {
        size_t operator()(const vve::NumType<T, P>& v) const {
            return hash<decltype(v.value)>()(v.value);
        }
    };

    template<>
    struct hash<vve::VeHandle> {
        size_t operator()(const vve::VeHandle& v) const {
            return hash<decltype(v.d_guid.value)>()(v.d_guid.value);
        }
    };

    template<>
    struct hash<vve::VeGuid> {
        size_t operator()(const vve::VeGuid& v) const {
            return hash<decltype(v.value)>()(v.value);
        }
    };

    template<>
    struct hash<vve::VeHash> {
        size_t operator()(const vve::VeHash& v) const {
            return v.value;
        }
    };

};


#endif

