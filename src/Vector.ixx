export module VEEngine:Vector;
import std;
import VEEngine.V4;

/**
 * @file
 * @brief Public vector facade backed by the selected engine implementation.
 */
export namespace vve {

   template <typename T>
   using Vector = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Vector<T>; ///< Facade dynamic array type.

   template <typename T>
   using VectorConstRange = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::VectorConstRange<T>; ///< Facade read-only vector range.

   using VVE_ENGINE_IMPLEMENTATION_NAMESPACE::makeRange; ///< Facade read-only vector range helper.

   template <typename TContainer, typename TValue> concept VectorLike =
      requires(TContainer container, const TContainer const_container, TValue value, std::size_t count,
               typename TContainer::const_iterator const_it) {
         typename TContainer::value_type;
         typename TContainer::size_type;
         typename TContainer::difference_type;
         typename TContainer::reference;
         typename TContainer::const_reference;
         typename TContainer::pointer;
         typename TContainer::const_pointer;
         typename TContainer::iterator;
         typename TContainer::const_iterator;
         { container.at(count) } -> std::same_as<typename TContainer::reference>;
         { const_container.at(count) } -> std::same_as<typename TContainer::const_reference>;
         { container[count] } -> std::same_as<typename TContainer::reference>;
         { const_container[count] } -> std::same_as<typename TContainer::const_reference>;
         { container.front() } -> std::same_as<typename TContainer::reference>;
         { const_container.front() } -> std::same_as<typename TContainer::const_reference>;
         { container.back() } -> std::same_as<typename TContainer::reference>;
         { const_container.back() } -> std::same_as<typename TContainer::const_reference>;
         { container.emplace_back(value) } -> std::same_as<typename TContainer::reference>;
         { container.push_back(value) } -> std::same_as<void>;
         { container.appendRange(std::initializer_list<TValue>{}) } -> std::same_as<void>;
         { container.emplace(const_it, value) } -> std::same_as<typename TContainer::iterator>;
         { container.insert(const_it, value) } -> std::same_as<typename TContainer::iterator>;
         { container.erase(const_it) } -> std::same_as<typename TContainer::iterator>;
         { container.pop_back() } -> std::same_as<void>;
         { container.clear() } -> std::same_as<void>;
         { container.reserve(count) } -> std::same_as<void>;
         { container.resize(count) } -> std::same_as<void>;
         { container.resize(count, value) } -> std::same_as<void>;
         { container.empty() } -> std::same_as<bool>;
         { container.size() } -> std::convertible_to<std::size_t>;
         { container.capacity() } -> std::convertible_to<std::size_t>;
         { container.segmentCount() } -> std::convertible_to<std::size_t>;
         { TContainer::segmentSize() } -> std::convertible_to<std::size_t>;
         { container.begin() };
         { const_container.begin() };
         { const_container.cbegin() };
         { container.end() };
         { const_container.end() };
         { const_container.cend() };
         { container.swap(container) } -> std::same_as<void>;
      }; ///< Contract for the public dynamic array alias.

   static_assert(VectorLike<Vector<int>, int>);
   static_assert(std::same_as<VectorConstRange<int>, decltype(makeRange(std::declval<const Vector<int> &>()))>);

} // namespace vve
