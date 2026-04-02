template <typename TImplementation>
std::expected<vve::Handle, vve::Error> vve::ECSFacade<TImplementation>::create() {
    return implementation_.create();
}

template <typename TImplementation>
std::expected<bool, vve::Error> vve::ECSFacade<TImplementation>::exists(vve::Handle entity) const {
    return implementation_.exists(entity);
}

template <typename TImplementation>
std::expected<void, vve::Error> vve::ECSFacade<TImplementation>::erase(vve::Handle entity) {
    return implementation_.erase(entity);
}

template class vve::ECSFacade<vve::v3::BasicECSImplementation<>>;
