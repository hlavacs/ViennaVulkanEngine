export module VEEngine.V3.Vector;
import std;

export namespace vve::v3 {

   template <typename T, std::size_t SegmentSize = 256> class Vector {
      static_assert(SegmentSize > 0, "SegmentSize must be greater than zero.");

   public:
      using value_type = T;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using reference = T &;
      using const_reference = const T &;
      using pointer = T *;
      using const_pointer = const T *;
      using allocator_type = std::allocator<T>;

   private:
      using allocator_traits = std::allocator_traits<allocator_type>;

      struct Segment {
         pointer data{nullptr};
      };

      template <typename TValue> class basic_iterator {
      public:
         using iterator_category = std::random_access_iterator_tag;
         using iterator_concept = std::random_access_iterator_tag;
         using value_type = std::remove_cv_t<TValue>;
         using difference_type = std::ptrdiff_t;
         using reference = TValue &;
         using pointer = TValue *;

         basic_iterator() = default;
         template <typename TOtherValue>
            requires(std::is_const_v<TValue> && std::same_as<std::remove_const_t<TValue>, TOtherValue>)
         basic_iterator(const basic_iterator<TOtherValue> &other) : owner_(other.owner_), index_(other.index_) {}

         [[nodiscard]] reference operator*() const { return (*owner_)[index_]; }
         [[nodiscard]] pointer operator->() const { return std::addressof((*owner_)[index_]); }

         basic_iterator &operator++() {
            ++index_;
            return *this;
         }

         basic_iterator operator++(int) {
            auto copy = *this;
            ++(*this);
            return copy;
         }

         basic_iterator &operator--() {
            --index_;
            return *this;
         }

         basic_iterator operator--(int) {
            auto copy = *this;
            --(*this);
            return copy;
         }

         basic_iterator &operator+=(difference_type offset) {
            index_ += offset;
            return *this;
         }

         basic_iterator &operator-=(difference_type offset) {
            index_ -= offset;
            return *this;
         }

         [[nodiscard]] reference operator[](difference_type offset) const { return *(*this + offset); }

         [[nodiscard]] friend basic_iterator operator+(basic_iterator it, difference_type offset) {
            it += offset;
            return it;
         }

         [[nodiscard]] friend basic_iterator operator+(difference_type offset, basic_iterator it) {
            it += offset;
            return it;
         }

         [[nodiscard]] friend basic_iterator operator-(basic_iterator it, difference_type offset) {
            it -= offset;
            return it;
         }

         [[nodiscard]] friend difference_type operator-(const basic_iterator &lhs, const basic_iterator &rhs) {
            return static_cast<difference_type>(lhs.index_) - static_cast<difference_type>(rhs.index_);
         }

         [[nodiscard]] friend bool operator==(const basic_iterator &, const basic_iterator &) = default;

         [[nodiscard]] friend auto operator<=>(const basic_iterator &lhs, const basic_iterator &rhs) {
            return lhs.index_ <=> rhs.index_;
         }

      private:
         using owner_type = std::conditional_t<std::is_const_v<TValue>, const Vector, Vector>;

         friend class Vector;
         template <typename> friend class basic_iterator;

         basic_iterator(owner_type *owner, size_type index) : owner_(owner), index_(index) {}

         owner_type *owner_{nullptr};
         difference_type index_{0};
      };

   public:
      using iterator = basic_iterator<T>;
      using const_iterator = basic_iterator<const T>;

      Vector() = default;

      Vector(std::initializer_list<T> values) {
         reserve(values.size());
         for (const auto &value : values) {
            push_back(value);
         }
      }

      explicit Vector(size_type count, const T &value = T{}) {
         resize(count, value);
      }

      template <std::ranges::input_range TRange>
         requires(std::constructible_from<T, std::ranges::range_reference_t<TRange>>)
      explicit Vector(std::from_range_t, TRange &&range) {
         appendRange(std::forward<TRange>(range));
      }

      Vector(const Vector &other) {
         reserve(other.size());
         for (const auto &value : other) {
            push_back(value);
         }
      }

      Vector(Vector &&other) noexcept
          : allocator_(std::move(other.allocator_)), segments_(std::move(other.segments_)), size_(other.size_) {
         other.size_ = 0;
      }

      Vector &operator=(const Vector &other) {
         if (this == &other) {
            return *this;
         }

         Vector copy(other);
         swap(copy);
         return *this;
      }

      Vector &operator=(Vector &&other) noexcept {
         if (this == &other) {
            return *this;
         }

         clear();
         releaseSegments();
         allocator_ = std::move(other.allocator_);
         segments_ = std::move(other.segments_);
         size_ = other.size_;
         other.size_ = 0;
         return *this;
      }

      ~Vector() {
         clear();
         releaseSegments();
      }

      [[nodiscard]] reference at(size_type index) {
         if (index >= size_) {
            throw std::out_of_range("Vector index out of range");
         }

         return (*this)[index];
      }

      [[nodiscard]] const_reference at(size_type index) const {
         if (index >= size_) {
            throw std::out_of_range("Vector index out of range");
         }

         return (*this)[index];
      }

      [[nodiscard]] reference operator[](size_type index) {
         const auto segment_index = index / SegmentSize;
         const auto element_index = index % SegmentSize;
         return segments_[segment_index].data[element_index];
      }

      [[nodiscard]] const_reference operator[](size_type index) const {
         const auto segment_index = index / SegmentSize;
         const auto element_index = index % SegmentSize;
         return segments_[segment_index].data[element_index];
      }

      [[nodiscard]] reference front() { return (*this)[0]; }
      [[nodiscard]] const_reference front() const { return (*this)[0]; }
      [[nodiscard]] reference back() { return (*this)[size_ - 1]; }
      [[nodiscard]] const_reference back() const { return (*this)[size_ - 1]; }

      template <typename... TArgs> reference emplace_back(TArgs &&...args) {
         ensureCapacityFor(size_ + 1);
         auto *slot = elementPointer(size_);
         allocator_traits::construct(allocator_, slot, std::forward<TArgs>(args)...);
         ++size_;
         return *slot;
      }

      void push_back(const T &value) { static_cast<void>(emplace_back(value)); }
      void push_back(T &&value) { static_cast<void>(emplace_back(std::move(value))); }

      template <std::ranges::input_range TRange>
         requires(std::constructible_from<T, std::ranges::range_reference_t<TRange>>)
      void appendRange(TRange &&range) {
         if constexpr (std::ranges::sized_range<TRange>) {
            reserve(size_ + static_cast<size_type>(std::ranges::size(range)));
         }

         for (auto &&value : range) {
            emplace_back(std::forward<decltype(value)>(value));
         }
      }

      template <typename... TArgs> iterator emplace(const_iterator position, TArgs &&...args) {
         const auto index = static_cast<size_type>(position.index_);
         if (index > size_) {
            throw std::out_of_range("Vector insert position out of range");
         }

         if (index == size_) {
            static_cast<void>(emplace_back(std::forward<TArgs>(args)...));
            return begin() + static_cast<difference_type>(index);
         }

         ensureCapacityFor(size_ + 1);
         for (size_type current = size_; current > index; --current) {
            if (current == size_) {
               allocator_traits::construct(allocator_, elementPointer(current), std::move((*this)[current - 1]));
            } else {
               (*this)[current] = std::move((*this)[current - 1]);
            }
         }

         (*this)[index] = T(std::forward<TArgs>(args)...);
         ++size_;
         return begin() + static_cast<difference_type>(index);
      }

      iterator insert(const_iterator position, const T &value) { return emplace(position, value); }
      iterator insert(const_iterator position, T &&value) { return emplace(position, std::move(value)); }

      iterator erase(const_iterator position) {
         const auto index = static_cast<size_type>(position.index_);
         if (index >= size_) {
            return end();
         }

         for (size_type current = index; current + 1 < size_; ++current) {
            (*this)[current] = std::move((*this)[current + 1]);
         }

         pop_back();
         return begin() + static_cast<difference_type>(index);
      }

      void pop_back() {
         if (size_ == 0) {
            return;
         }

         --size_;
         allocator_traits::destroy(allocator_, elementPointer(size_));
      }

      void clear() noexcept {
         while (size_ > 0) {
            pop_back();
         }
      }

      void reserve(size_type new_capacity) {
         const auto required_segments = segmentsFor(new_capacity);
         while (segments_.size() < required_segments) {
            allocateSegment();
         }
      }

      void resize(size_type new_size) {
         if (new_size < size_) {
            while (size_ > new_size) {
               pop_back();
            }
            return;
         }

         reserve(new_size);
         while (size_ < new_size) {
            static_cast<void>(emplace_back());
         }
      }

      void resize(size_type new_size, const T &value) {
         if (new_size < size_) {
            while (size_ > new_size) {
               pop_back();
            }
            return;
         }

         reserve(new_size);
         while (size_ < new_size) {
            push_back(value);
         }
      }

      [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
      [[nodiscard]] size_type size() const noexcept { return size_; }
      [[nodiscard]] size_type capacity() const noexcept { return segments_.size() * SegmentSize; }
      [[nodiscard]] size_type segmentCount() const noexcept { return segments_.size(); }
      [[nodiscard]] static consteval size_type segmentSize() noexcept { return SegmentSize; }

      [[nodiscard]] iterator begin() noexcept { return iterator(this, 0); }
      [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(this, 0); }
      [[nodiscard]] const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
      [[nodiscard]] iterator end() noexcept { return iterator(this, size_); }
      [[nodiscard]] const_iterator end() const noexcept { return const_iterator(this, size_); }
      [[nodiscard]] const_iterator cend() const noexcept { return const_iterator(this, size_); }

      void swap(Vector &other) noexcept {
         std::swap(allocator_, other.allocator_);
         segments_.swap(other.segments_);
         std::swap(size_, other.size_);
      }

   private:
      [[nodiscard]] static constexpr size_type segmentsFor(size_type count) noexcept {
         return count == 0 ? 0 : ((count - 1) / SegmentSize) + 1;
      }

      void ensureCapacityFor(size_type count) {
         if (count > capacity()) {
            reserve(std::max(count, capacity() + SegmentSize));
         }
      }

      [[nodiscard]] pointer elementPointer(size_type index) {
         const auto segment_index = index / SegmentSize;
         const auto element_index = index % SegmentSize;
         return segments_[segment_index].data + element_index;
      }

      [[nodiscard]] const_pointer elementPointer(size_type index) const {
         const auto segment_index = index / SegmentSize;
         const auto element_index = index % SegmentSize;
         return segments_[segment_index].data + element_index;
      }

      void allocateSegment() {
         segments_.push_back(Segment{.data = allocator_traits::allocate(allocator_, SegmentSize)});
      }

      void releaseSegments() noexcept {
         for (auto &segment : segments_) {
            if (segment.data != nullptr) {
               allocator_traits::deallocate(allocator_, segment.data, SegmentSize);
               segment.data = nullptr;
            }
         }

         segments_.clear();
      }

      allocator_type allocator_{};
      std::vector<Segment> segments_{};
      size_type size_{0};
   };

   template <typename T> using VectorConstRange = std::ranges::subrange<typename Vector<T>::const_iterator>;

   template <typename T> [[nodiscard]] VectorConstRange<T> makeRange(const Vector<T> &values) {
      return VectorConstRange<T>(values.cbegin(), values.cend());
   }

} // namespace vve::v3
