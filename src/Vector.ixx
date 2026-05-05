export module VEEngine:Vector;
import std;
import VEEngine.V4;

/**
 * @file
 * @brief Public vector facade backed by the selected engine implementation.
 */
export namespace vve {

   template <typename T> class Vector {
   public:
      using implementation_type = VVE_ENGINE_IMPLEMENTATION_NAMESPACE::Vector<T>;
      using value_type = typename implementation_type::value_type;
      using size_type = typename implementation_type::size_type;
      using difference_type = typename implementation_type::difference_type;
      using reference = typename implementation_type::reference;
      using const_reference = typename implementation_type::const_reference;
      using pointer = typename implementation_type::pointer;
      using const_pointer = typename implementation_type::const_pointer;
      using iterator = typename implementation_type::iterator;
      using const_iterator = typename implementation_type::const_iterator;

      Vector() = default;
      Vector(implementation_type implementation) : impl_{std::move(implementation)} {}
      Vector(std::initializer_list<T> values) : impl_{values} {}
      explicit Vector(size_type count, const T &value = T{}) : impl_{count, value} {}
      template <std::ranges::input_range TRange>
         requires(std::constructible_from<T, std::ranges::range_reference_t<TRange>>)
      explicit Vector(std::from_range_t, TRange &&range) : impl_{std::from_range, std::forward<TRange>(range)} {}
      Vector(const Vector &) = default;
      Vector(Vector &&) noexcept = default;
      Vector &operator=(const Vector &) = default;
      Vector &operator=(Vector &&) noexcept = default;

      [[nodiscard]] reference at(size_type index) { return impl_.at(index); }
      [[nodiscard]] const_reference at(size_type index) const { return impl_.at(index); }
      [[nodiscard]] reference operator[](size_type index) { return impl_[index]; }
      [[nodiscard]] const_reference operator[](size_type index) const { return impl_[index]; }
      [[nodiscard]] reference front() { return impl_.front(); }
      [[nodiscard]] const_reference front() const { return impl_.front(); }
      [[nodiscard]] reference back() { return impl_.back(); }
      [[nodiscard]] const_reference back() const { return impl_.back(); }

      template <typename... TArgs> reference emplace_back(TArgs &&...args) {
         return impl_.emplace_back(std::forward<TArgs>(args)...);
      }
      void push_back(const T &value) { impl_.push_back(value); }
      void push_back(T &&value) { impl_.push_back(std::move(value)); }
      template <std::ranges::input_range TRange>
         requires(std::constructible_from<T, std::ranges::range_reference_t<TRange>>)
      void appendRange(TRange &&range) {
         impl_.appendRange(std::forward<TRange>(range));
      }
      template <typename... TArgs> iterator emplace(const_iterator position, TArgs &&...args) {
         return impl_.emplace(position, std::forward<TArgs>(args)...);
      }
      iterator insert(const_iterator position, const T &value) { return impl_.insert(position, value); }
      iterator insert(const_iterator position, T &&value) { return impl_.insert(position, std::move(value)); }
      iterator erase(const_iterator position) { return impl_.erase(position); }
      void pop_back() { impl_.pop_back(); }
      void clear() noexcept { impl_.clear(); }
      void reserve(size_type new_capacity) { impl_.reserve(new_capacity); }
      void resize(size_type new_size) { impl_.resize(new_size); }
      void resize(size_type new_size, const T &value) { impl_.resize(new_size, value); }

      [[nodiscard]] bool empty() const noexcept { return impl_.empty(); }
      [[nodiscard]] size_type size() const noexcept { return impl_.size(); }
      [[nodiscard]] size_type capacity() const noexcept { return impl_.capacity(); }
      [[nodiscard]] size_type segmentCount() const noexcept { return impl_.segmentCount(); }
      [[nodiscard]] static consteval size_type segmentSize() noexcept { return implementation_type::segmentSize(); }
      [[nodiscard]] iterator begin() noexcept { return impl_.begin(); }
      [[nodiscard]] const_iterator begin() const noexcept { return impl_.begin(); }
      [[nodiscard]] const_iterator cbegin() const noexcept { return impl_.cbegin(); }
      [[nodiscard]] iterator end() noexcept { return impl_.end(); }
      [[nodiscard]] const_iterator end() const noexcept { return impl_.end(); }
      [[nodiscard]] const_iterator cend() const noexcept { return impl_.cend(); }
      void swap(Vector &other) noexcept { impl_.swap(other.impl_); }

      [[nodiscard]] implementation_type &implementation() noexcept { return impl_; }
      [[nodiscard]] const implementation_type &implementation() const noexcept { return impl_; }
      [[nodiscard]] operator implementation_type &() noexcept { return impl_; }
      [[nodiscard]] operator const implementation_type &() const noexcept { return impl_; }

   private:
      implementation_type impl_{};
   }; ///< Facade dynamic array type.

   template <typename T>
   using VectorConstRange = std::ranges::subrange<typename Vector<T>::const_iterator>; ///< Facade read-only vector range.

   template <typename T> [[nodiscard]] VectorConstRange<T> makeRange(const Vector<T> &values) {
      return VectorConstRange<T>(values.cbegin(), values.cend());
   } ///< Facade read-only vector range helper.

} // namespace vve
