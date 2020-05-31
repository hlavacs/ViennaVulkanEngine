#ifndef VETYPES_H
#define VETYPES_H



namespace std {

    template<typename... Args>
    struct hash<tuple<Args...>> {
        size_t operator()(const tuple<Args...> & v) {
            return vve::hash_impl(v, make_index_sequence<sizeof...(Args)>());
        }
    };

    template<typename T, typename P>
    struct hash<vve::IntType<T,P>> {
        size_t operator()(const vve::IntType<T, P>& v) const {
            return hash<decltype(v.value)>()(v.value);
        }
    };

    template<>
    struct hash<vve::VeHandle> {
        size_t operator()(const vve::VeHandle& v) const {
            return hash<decltype(v.d_guid)>()(v.d_guid);
        }
    };
};


#endif

