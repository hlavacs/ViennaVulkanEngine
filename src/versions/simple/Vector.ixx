export module VEEngine.V5.Vector;
import std;

/**
	* @file
	* @brief Segmented vector container used by v5 runtime data structures.
	*
	* The container trades a fully contiguous allocation for stable segment growth
	* that avoids large reallocations as runtime arrays expand.
	*/
export namespace vve::v5 {

	/**
		* @brief Segmented vector with random-access iteration.
		* @tparam T Element type.
		* @tparam SegmentSize Number of elements per segment allocation.
		*/
	template <typename T, std::size_t SegmentSize = 256> class Vector {
		static_assert(SegmentSize > 0, "SegmentSize must be greater than zero.");

	public:
		using value_type = T;									///< Element type stored by the segmented container.
		using size_type = std::size_t;						///< Unsigned size type used for counts and indices.
		using difference_type = std::ptrdiff_t;			///< Signed difference type used by iterators.
		using reference = T &;									///< Mutable element reference type.
		using const_reference = const T &;					///< Immutable element reference type.
		using pointer = T *;										///< Mutable element pointer type.
		using const_pointer = const T *;						///< Immutable element pointer type.
		using allocator_type = std::allocator<T>;			///< Allocator used for element storage inside each segment.

	private:
		/// @brief Allocator traits used for element construction and destruction.
		using allocator_traits = std::allocator_traits<allocator_type>;

		/// @brief One allocated element segment.
		struct Segment {
			pointer data{nullptr};																						///< Pointer to the first element slot in the segment.
		};

		/**
			* @brief Random-access iterator shared by mutable and const iterator variants.
			* @tparam TValue Iterator element cv-qualified type.
			*/
		template <typename TValue> class basic_iterator {
		public:
			using iterator_category = std::random_access_iterator_tag;										///< Legacy iterator category tag.
			using iterator_concept = std::random_access_iterator_tag;										///< C++20 iterator concept tag.
			using value_type = std::remove_cv_t<TValue>;															///< Unqualified iterator value type.
			using difference_type = std::ptrdiff_t;																///< Signed distance type between iterators.
			using reference = TValue &;																				///< Iterator dereference reference type.
			using pointer = TValue *;																					///< Iterator dereference pointer type.

			/// @brief Creates a default end/null iterator.
			basic_iterator() = default;
			/**
				* @brief Converts a mutable iterator into a const iterator.
				* @tparam TOtherValue Source iterator value type.
				* @param other Iterator to convert from.
				*/
			template <typename TOtherValue>
				requires(std::is_const_v<TValue> && std::same_as<std::remove_const_t<TValue>, TOtherValue>)
			basic_iterator(const basic_iterator<TOtherValue> &other) : owner_(other.owner_), index_(other.index_) {}

			/// @brief Returns a reference to the pointed-to element.
			[[nodiscard]] reference operator*() const { return (*owner_)[index_]; }
			/// @brief Returns a pointer to the pointed-to element.
			[[nodiscard]] pointer operator->() const { return std::addressof((*owner_)[index_]); }

			/// @brief Advances the iterator by one element.
			basic_iterator &operator++() {
				++index_;
				return *this;
			}

			/// @brief Returns the old iterator value and advances by one element.
			basic_iterator operator++(int) {
				auto copy = *this;
				++(*this);
				return copy;
			}

			/// @brief Moves the iterator back by one element.
			basic_iterator &operator--() {
				--index_;
				return *this;
			}

			/// @brief Returns the old iterator value and moves back by one element.
			basic_iterator operator--(int) {
				auto copy = *this;
				--(*this);
				return copy;
			}

			/// @brief Advances the iterator by `offset` elements.
			basic_iterator &operator+=(difference_type offset) {
				index_ += offset;
				return *this;
			}

			/// @brief Moves the iterator back by `offset` elements.
			basic_iterator &operator-=(difference_type offset) {
				index_ -= offset;
				return *this;
			}

			/// @brief Returns the element at `offset` relative to the iterator.
			[[nodiscard]] reference operator[](difference_type offset) const { return *(*this + offset); }

			/// @brief Returns an iterator advanced by `offset`.
			[[nodiscard]] friend basic_iterator operator+(basic_iterator it, difference_type offset) {
				it += offset;
				return it;
			}

			/// @brief Returns an iterator advanced by `offset`.
			[[nodiscard]] friend basic_iterator operator+(difference_type offset, basic_iterator it) {
				it += offset;
				return it;
			}

			/// @brief Returns an iterator moved back by `offset`.
			[[nodiscard]] friend basic_iterator operator-(basic_iterator it, difference_type offset) {
				it -= offset;
				return it;
			}

			/// @brief Returns the signed distance between two iterators.
			[[nodiscard]] friend difference_type operator-(const basic_iterator &lhs, const basic_iterator &rhs) {
				return static_cast<difference_type>(lhs.index_) - static_cast<difference_type>(rhs.index_);
			}

			/// @brief Returns whether two iterators point at the same owner/index pair.
			[[nodiscard]] friend bool operator==(const basic_iterator &, const basic_iterator &) = default;

			/// @brief Compares iterator positions within the same container.
			[[nodiscard]] friend bool operator<(const basic_iterator &lhs, const basic_iterator &rhs) {
				return lhs.index_ < rhs.index_;
			}

			/// @brief Compares iterator positions within the same container.
			[[nodiscard]] friend bool operator>(const basic_iterator &lhs, const basic_iterator &rhs) {
				return rhs < lhs;
			}

			/// @brief Compares iterator positions within the same container.
			[[nodiscard]] friend bool operator<=(const basic_iterator &lhs, const basic_iterator &rhs) {
				return !(rhs < lhs);
			}

			/// @brief Compares iterator positions within the same container.
			[[nodiscard]] friend bool operator>=(const basic_iterator &lhs, const basic_iterator &rhs) {
				return !(lhs < rhs);
			}

		private:
			using owner_type = std::conditional_t<std::is_const_v<TValue>, const Vector, Vector>;	///< Owning container type for this iterator flavor.

			friend class Vector;
			template <typename> friend class basic_iterator;

			/// @brief Creates an iterator over `owner` positioned at `index`.
			basic_iterator(owner_type *owner, size_type index) : owner_(owner), index_(index) {}

			owner_type *owner_{nullptr};																				///< Container being iterated.
			difference_type index_{0};																					///< Logical element index inside the container.
		};

	public:
		using iterator = basic_iterator<T>;					///< Mutable random-access iterator type.
		using const_iterator = basic_iterator<const T>;	///< Immutable random-access iterator type.

		/// @brief Creates an empty segmented vector.
		Vector() = default;

		/**
			* @brief Creates a segmented vector from an initializer list.
			* @param values Elements copied into the container.
			*/
		Vector(std::initializer_list<T> values) {
			reserve(values.size());
			for (const auto &value : values) {
				push_back(value);
			}
		}

		/**
			* @brief Creates a segmented vector with `count` copies of `value`.
			* @param count Number of elements to create.
			* @param value Fill value copied into each element.
			*/
		explicit Vector(size_type count, const T &value = T{}) {
			resize(count, value);
		}

		/**
			* @brief Creates a segmented vector by appending elements from a range.
			* @tparam TRange Source range type.
			* @param range Source range consumed by the constructor.
			*/
		template <std::ranges::input_range TRange>
			requires(std::constructible_from<T, std::ranges::range_reference_t<TRange>>)
		explicit Vector(std::from_range_t, TRange &&range) {
			appendRange(std::forward<TRange>(range));
		}

		/// @brief Creates a deep copy of another segmented vector.
		Vector(const Vector &other) {
			reserve(other.size());
			for (const auto &value : other) {
				push_back(value);
			}
		}

		/// @brief Moves segment ownership from `other`.
		Vector(Vector &&other) noexcept
				: allocator_(std::move(other.allocator_)), segments_(std::move(other.segments_)), size_(other.size_) {
			other.size_ = 0;
			other.segments_.clear();
		}

		/// @brief Replaces this container with a deep copy of `other`.
		Vector &operator=(const Vector &other) {
			if (this == &other) {
				return *this;
			}

			Vector copy(other);
			swap(copy);
			return *this;
		}

		/// @brief Moves all state from `other` into this container.
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
			other.segments_.clear();
			return *this;
		}

		/// @brief Destroys all elements and releases all segment allocations.
		~Vector() {
			clear();
			releaseSegments();
		}

		/// @brief Returns the element at `index` with bounds checking.
		[[nodiscard]] auto at(size_type index)													-> reference{
			if (index >= size_) {
				throw std::out_of_range("Vector index out of range");
			}

			return (*this)[index];
		}

		/// @brief Returns the element at `index` with bounds checking.
		[[nodiscard]] auto at(size_type index) const											-> const_reference{
			if (index >= size_) {
				throw std::out_of_range("Vector index out of range");
			}

			return (*this)[index];
		}

		/// @brief Returns the element at `index` without bounds checking.
		[[nodiscard]] reference operator[](size_type index) {
			const auto segment_index = index / SegmentSize;
			const auto element_index = index % SegmentSize;
			return segments_[segment_index].data[element_index];
		}

		/// @brief Returns the element at `index` without bounds checking.
		[[nodiscard]] const_reference operator[](size_type index) const {
			const auto segment_index = index / SegmentSize;
			const auto element_index = index % SegmentSize;
			return segments_[segment_index].data[element_index];
		}

		/// @brief Returns the first element.
		[[nodiscard]] reference front() { return (*this)[0]; }
		/// @brief Returns the first element.
		[[nodiscard]] const_reference front() const { return (*this)[0]; }
		/// @brief Returns the last element.
		[[nodiscard]] reference back() { return (*this)[size_ - 1]; }
		/// @brief Returns the last element.
		[[nodiscard]] const_reference back() const { return (*this)[size_ - 1]; }

		/**
			* @brief Constructs an element at the end of the container.
			* @tparam TArgs Constructor argument types.
			* @param args Arguments forwarded to the element constructor.
			* @return Reference to the newly created element.
			*/
		template <typename... TArgs> reference emplace_back(TArgs &&...args) {
			// Segment allocation happens lazily so pushes remain amortized without
			// requiring one large contiguous buffer.
			ensureCapacityFor(size_ + 1);
			auto *slot = elementPointer(size_);
			allocator_traits::construct(allocator_, slot, std::forward<TArgs>(args)...);
			++size_;
			return *slot;
		}

		/// @brief Appends a copy of `value`.
		void push_back(const T &value) { static_cast<void>(emplace_back(value)); }
		/// @brief Appends `value` by move.
		void push_back(T &&value) { static_cast<void>(emplace_back(std::move(value))); }

		/**
			* @brief Appends all elements from a source range.
			* @tparam TRange Source range type.
			* @param range Source range to append.
			*/
		template <std::ranges::input_range TRange>
			requires(std::constructible_from<T, std::ranges::range_reference_t<TRange>>)
		auto appendRange(TRange &&range)															-> void{
			if constexpr (std::ranges::sized_range<TRange>) {
				reserve(size_ + static_cast<size_type>(std::ranges::size(range)));
			}

			for (auto &&value : range) {
				emplace_back(std::forward<decltype(value)>(value));
			}
		}

		/**
			* @brief Inserts an element before `position`.
			* @tparam TArgs Constructor argument types.
			* @param position Insertion point.
			* @param args Arguments forwarded to the inserted element constructor.
			* @return Iterator pointing at the inserted element.
			*/
		template <typename... TArgs> iterator emplace(const_iterator position, TArgs &&...args) {
			const auto index = static_cast<size_type>(position.index_);
			if (index > size_) {
				throw std::out_of_range("Vector insert position out of range");
			}

			if (index == size_) {
				static_cast<void>(emplace_back(std::forward<TArgs>(args)...));
				return begin() + static_cast<difference_type>(index);
			}

			// Elements are shifted one slot to the right within the segmented
			// logical index space to make room for the new element.
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

		/// @brief Inserts a copy of `value` before `position`.
		iterator insert(const_iterator position, const T &value) { return emplace(position, value); }
		/// @brief Inserts `value` by move before `position`.
		iterator insert(const_iterator position, T &&value) { return emplace(position, std::move(value)); }

		/**
			* @brief Removes the element at `position`.
			* @param position Iterator naming the element to erase.
			* @return Iterator to the element that followed the erased one.
			*/
		auto erase(const_iterator position)														-> iterator{
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

		/// @brief Removes the last element when the container is non-empty.
		auto pop_back()																				-> void{
			if (size_ == 0) {
				return;
			}

			--size_;
			allocator_traits::destroy(allocator_, elementPointer(size_));
		}

		/// @brief Destroys all constructed elements but keeps allocated segments.
		auto clear() noexcept																		-> void{
			while (size_ > 0) {
				pop_back();
			}
		}

		/// @brief Ensures capacity for at least `new_capacity` elements.
		auto reserve(size_type new_capacity)													-> void{
			const auto required_segments = segmentsFor(new_capacity);
			while (segments_.size() < required_segments) {
				allocateSegment();
			}
		}

		/// @brief Resizes the container, value-initializing new elements when it grows.
		auto resize(size_type new_size)															-> void{
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

		/// @brief Resizes the container, copying `value` into new elements when it grows.
		auto resize(size_type new_size, const T &value)										-> void{
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

		/// @brief Returns whether the container is empty.
		[[nodiscard]] bool empty() const noexcept { return size_ == 0; }
		/// @brief Returns the number of constructed elements.
		[[nodiscard]] size_type size() const noexcept { return size_; }
		/// @brief Returns the number of elements that fit in the currently allocated segments.
		[[nodiscard]] size_type capacity() const noexcept { return segments_.size() * SegmentSize; }
		/// @brief Returns the number of allocated segments.
		[[nodiscard]] size_type segmentCount() const noexcept { return segments_.size(); }
		/// @brief Returns the compile-time segment size.
		[[nodiscard]] static consteval size_type segmentSize() noexcept { return SegmentSize; }

		/// @brief Returns an iterator to the first element.
		[[nodiscard]] iterator begin() noexcept { return iterator(this, 0); }
		/// @brief Returns an iterator to the first element.
		[[nodiscard]] const_iterator begin() const noexcept { return const_iterator(this, 0); }
		/// @brief Returns a const iterator to the first element.
		[[nodiscard]] const_iterator cbegin() const noexcept { return const_iterator(this, 0); }
		/// @brief Returns an iterator one past the last element.
		[[nodiscard]] iterator end() noexcept { return iterator(this, size_); }
		/// @brief Returns an iterator one past the last element.
		[[nodiscard]] const_iterator end() const noexcept { return const_iterator(this, size_); }
		/// @brief Returns a const iterator one past the last element.
		[[nodiscard]] const_iterator cend() const noexcept { return const_iterator(this, size_); }

		/// @brief Exchanges all storage state with `other`.
		auto swap(Vector &other) noexcept														-> void{
			std::swap(allocator_, other.allocator_);
			segments_.swap(other.segments_);
			std::swap(size_, other.size_);
		}

	private:
		/// @brief Returns how many segments are required to store `count` elements.
		[[nodiscard]] static constexpr auto segmentsFor(size_type count) noexcept	-> size_type{
			return count == 0 ? 0 : ((count - 1) / SegmentSize) + 1;
		}

		/// @brief Ensures the container can hold `count` elements.
		auto ensureCapacityFor(size_type count)												-> void{
			if (count > capacity()) {
				reserve(std::max(count, capacity() + SegmentSize));
			}
		}

		/// @brief Returns a pointer to the element slot at logical `index`.
		[[nodiscard]] auto elementPointer(size_type index)									-> pointer{
			const auto segment_index = index / SegmentSize;
			const auto element_index = index % SegmentSize;
			return segments_[segment_index].data + element_index;
		}

		/// @brief Returns a pointer to the element slot at logical `index`.
		[[nodiscard]] auto elementPointer(size_type index) const							-> const_pointer{
			const auto segment_index = index / SegmentSize;
			const auto element_index = index % SegmentSize;
			return segments_[segment_index].data + element_index;
		}

		/// @brief Allocates and appends one new segment.
		auto allocateSegment()																		-> void{
			segments_.push_back(Segment{.data = allocator_traits::allocate(allocator_, SegmentSize)});
		}

		/// @brief Releases all allocated segments after elements have been destroyed.
		auto releaseSegments() noexcept															-> void{
			for (auto &segment : segments_) {
				if (segment.data != nullptr) {
					allocator_traits::deallocate(allocator_, segment.data, SegmentSize);
					segment.data = nullptr;
				}
			}

			segments_.clear();
		}

		allocator_type allocator_{};							///< Allocator instance used for segment allocations.
		std::vector<Segment> segments_{};					///< Owned segment descriptors in allocation order.
		size_type size_{0};										///< Number of constructed elements currently stored.
	};

	/// @brief Read-only range alias spanning the full logical contents of a segmented vector.
	template <typename T> using VectorConstRange = std::ranges::subrange<typename Vector<T>::const_iterator>;

	/**
		* @brief Returns a const range covering the full contents of `values`.
		* @tparam T Element type.
		* @param values Segmented vector to expose as a range.
		* @return Subrange spanning `[cbegin(), cend())`.
		*/
	template <typename T> [[nodiscard]] VectorConstRange<T> makeRange(const Vector<T> &values) {
		return VectorConstRange<T>(values.cbegin(), values.cend());
	}

} // namespace vve::v5
