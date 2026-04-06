#include <algorithm>
#include <memory>
#include <numeric>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

import VEEngine.V3;

/**
 * @file
 * @brief Regression tests for the segmented v3 `Vector` container.
 */
namespace {

   using int_container = vve::v3::Vector<int, 4>; ///< Test alias using a small segment size to force multi-segment behavior quickly.

   /// @brief Small aggregate used to test emplacement of non-scalar values.
   struct pair_value {
      int first{0};  ///< First stored integer.
      int second{0}; ///< Second stored integer.

      /// @brief Creates the default zero pair.
      pair_value() = default;
      /// @brief Creates a pair from explicit integer values.
      pair_value(int first_value, int second_value) : first(first_value), second(second_value) {}

      /// @brief Compares two pair values field-by-field.
      [[nodiscard]] friend bool operator==(const pair_value &, const pair_value &) = default;
   };

   /// @brief Value type that tracks lifetime, copy, and move activity for container tests.
   struct tracked_value {
      inline static int alive_count = 0; ///< Number of currently alive instances.
      inline static int copy_count = 0;  ///< Number of copy operations observed.
      inline static int move_count = 0;  ///< Number of move operations observed.

      int value{0}; ///< Stored payload value.

      /// @brief Creates a tracked value with default payload.
      tracked_value() { ++alive_count; }
      /// @brief Creates a tracked value with explicit payload.
      explicit tracked_value(int initial_value) : value(initial_value) { ++alive_count; }

      /// @brief Copy-constructs a tracked value and increments counters.
      tracked_value(const tracked_value &other) : value(other.value) {
         ++alive_count;
         ++copy_count;
      }

      /// @brief Move-constructs a tracked value and marks the source as moved-from.
      tracked_value(tracked_value &&other) noexcept : value(other.value) {
         ++alive_count;
         ++move_count;
         other.value = -1;
      }

      /// @brief Copy-assigns the payload and increments the copy counter.
      tracked_value &operator=(const tracked_value &other) {
         value = other.value;
         ++copy_count;
         return *this;
      }

      /// @brief Move-assigns the payload and marks the source as moved-from.
      tracked_value &operator=(tracked_value &&other) noexcept {
         value = other.value;
         ++move_count;
         other.value = -1;
         return *this;
      }

      /// @brief Destroys the tracked value and decrements the live-instance counter.
      ~tracked_value() { --alive_count; }

      /// @brief Compares two tracked values by payload.
      [[nodiscard]] friend bool operator==(const tracked_value &, const tracked_value &) = default;

      /// @brief Resets all static test counters.
      static void resetCounters() {
         alive_count = 0;
         copy_count = 0;
         move_count = 0;
      }
   };

   /// @brief Verifies that `values` matches an expected integer sequence exactly.
   [[nodiscard]] bool verifySequence(const int_container &values, std::initializer_list<int> expected) {
      if (values.size() != expected.size()) {
         return false;
      }

      return std::ranges::equal(values, expected);
   }

   /// @brief Tests basic growth, random access, and stable-address behavior.
   [[nodiscard]] int testBasicGrowthAndAccess() {
      int_container values{};
      if (!values.empty() || values.size() != 0 || values.capacity() != 0 || values.segmentCount() != 0) {
         return 1;
      }

      for (int value = 0; value < 10; ++value) {
         values.push_back(value * 10);
      }

      if (values.size() != 10 || values.segmentCount() != 3 || values.capacity() != 12) {
         return 2;
      }

      for (std::size_t index = 0; index < values.size(); ++index) {
         if (values[index] != static_cast<int>(index * 10)) {
            return 3;
         }
      }

      if (values.front() != 0 || values.back() != 90 || values.at(4) != 40) {
         return 4;
      }

      auto *stable_address = std::addressof(values[1]);
      const int stable_value = values[1];
      values.push_back(100);
      values.push_back(110);
      values.push_back(120);
      if (std::addressof(values[1]) != stable_address || values[1] != stable_value) {
         return 5;
      }

      return 0;
   }

   /// @brief Tests insertion, erasure, resizing, and clearing across segment boundaries.
   [[nodiscard]] int testInsertEraseAndResize() {
      int_container values{};
      for (int value = 0; value < 10; ++value) {
         values.push_back(value * 10);
      }

      const auto insert_it = values.insert(values.cbegin() + 3, 999);
      if (insert_it == values.end() || *insert_it != 999 || values[3] != 999 || values.size() != 11) {
         return 1;
      }

      const auto begin_insert_it = values.insert(values.cbegin(), -10);
      if (begin_insert_it != values.begin() || values.front() != -10 || values.size() != 12) {
         return 2;
      }

      const auto end_insert_it = values.insert(values.cend(), 200);
      if (end_insert_it != values.end() - 1 || values.back() != 200 || values.size() != 13) {
         return 3;
      }

      const auto erase_middle_it = values.erase(values.cbegin() + 4);
      if (erase_middle_it == values.end() || *erase_middle_it != 30 || values.size() != 12) {
         return 4;
      }

      const auto erase_last_it = values.erase(values.cend() - 1);
      if (erase_last_it != values.end() || values.back() != 90 || values.size() != 11) {
         return 5;
      }

      const auto erase_end_it = values.erase(values.cend());
      if (erase_end_it != values.end() || values.size() != 11) {
         return 6;
      }

      values.resize(16, -1);
      if (values.size() != 16 || values[11] != -1 || values[15] != -1 || values.segmentCount() != 4) {
         return 7;
      }

      values.resize(6);
      if (values.size() != 6 || values.back() != 40) {
         return 8;
      }

      values.pop_back();
      if (values.size() != 5 || values.back() != 30) {
         return 9;
      }

      values.clear();
      if (!values.empty() || values.size() != 0 || values.capacity() != 16) {
         return 10;
      }

      values.pop_back();
      if (!values.empty()) {
         return 11;
      }

      return 0;
   }

   /// @brief Tests explicit reserve behavior and fill/default construction paths.
   [[nodiscard]] int testReserveAndConstruction() {
      int_container values{};
      values.reserve(1);
      if (values.capacity() != 4 || values.segmentCount() != 1) {
         return 1;
      }

      values.reserve(4);
      if (values.capacity() != 4 || values.segmentCount() != 1) {
         return 2;
      }

      values.reserve(9);
      if (values.capacity() != 12 || values.segmentCount() != 3) {
         return 3;
      }

      values.reserve(2);
      if (values.capacity() != 12 || values.segmentCount() != 3) {
         return 4;
      }

      int_container default_values(6);
      if (default_values.size() != 6 || !verifySequence(default_values, {0, 0, 0, 0, 0, 0})) {
         return 5;
      }

      int_container filled_values(6, 7);
      if (filled_values.size() != 6 || !verifySequence(filled_values, {7, 7, 7, 7, 7, 7})) {
         return 6;
      }

      filled_values.resize(8);
      if (!verifySequence(filled_values, {7, 7, 7, 7, 7, 7, 0, 0})) {
         return 7;
      }

      return 0;
   }

   /// @brief Tests iterator correctness, const access, and range compatibility.
   [[nodiscard]] int testIteratorsAndConstAccess() {
      int_container values{};
      for (int value = 1; value <= 8; ++value) {
         values.push_back(value);
      }

      const int_container &const_values = values;
      const auto distance = const_values.cend() - const_values.cbegin();
      if (distance != static_cast<int_container::difference_type>(const_values.size())) {
         return 1;
      }

      if (*(const_values.cbegin() + 5) != 6) {
         return 2;
      }

      if ((const_values.cend() - 3)[0] != 6) {
         return 3;
      }

      const auto reverse_sum =
          std::accumulate(std::make_reverse_iterator(values.end()), std::make_reverse_iterator(values.begin()), 0);
      if (reverse_sum != 36) {
         return 4;
      }

      const auto even_count = std::ranges::count_if(const_values, [](int value) { return (value % 2) == 0; });
      if (even_count != 4) {
         return 5;
      }

      std::vector<int> copied_values{};
      copied_values.reserve(values.size());
      std::ranges::copy(values, std::back_inserter(copied_values));
      if (!std::ranges::equal(values, copied_values)) {
         return 6;
      }

      if (!(values.begin() < values.end()) || !(values.begin() + 4 > values.begin() + 2)) {
         return 7;
      }

      return 0;
   }

   /// @brief Tests copy/move construction, assignment, and swap semantics.
   [[nodiscard]] int testCopyMoveSwapAndAssignment() {
      int_container original{};
      for (int value = 0; value < 9; ++value) {
         original.push_back(value);
      }

      int_container copied(original);
      if (!std::ranges::equal(original, copied) || copied.capacity() != 12) {
         return 1;
      }

      copied[0] = 99;
      if (original[0] != 0) {
         return 2;
      }

      int_container assigned{};
      assigned = original;
      if (!std::ranges::equal(original, assigned)) {
         return 3;
      }

      assigned = assigned;
      if (!std::ranges::equal(original, assigned)) {
         return 4;
      }

      auto *stable_pointer = std::addressof(original[2]);
      const int stable_value = original[2];
      int_container moved(std::move(original));
      if (!std::ranges::equal(moved, std::views::iota(0, 9)) || moved.size() != 9) {
         return 5;
      }

      if (std::addressof(moved[2]) != stable_pointer || moved[2] != stable_value) {
         return 6;
      }

      if (!original.empty() || original.size() != 0) {
         return 7;
      }

      int_container move_assigned{};
      move_assigned = std::move(moved);
      if (!std::ranges::equal(move_assigned, std::views::iota(0, 9)) || !moved.empty()) {
         return 8;
      }

      int_container left{};
      left.push_back(1);
      left.push_back(2);
      int_container right{};
      right.push_back(7);
      right.push_back(8);
      right.push_back(9);
      left.swap(right);
      if (!verifySequence(left, {7, 8, 9}) || !verifySequence(right, {1, 2})) {
         return 9;
      }

      return 0;
   }

   /// @brief Tests bounds-checked access on mutable and const containers.
   [[nodiscard]] int testOutOfRangeAndConstAt() {
      int_container values{};
      values.push_back(5);
      values.push_back(6);

      const int_container &const_values = values;
      if (const_values.at(1) != 6) {
         return 1;
      }

      bool threw = false;
      try {
         static_cast<void>(values.at(2));
      } catch (const std::out_of_range &) {
         threw = true;
      }

      if (!threw) {
         return 2;
      }

      threw = false;
      try {
         static_cast<void>(const_values.at(3));
      } catch (const std::out_of_range &) {
         threw = true;
      }

      if (!threw) {
         return 3;
      }

      return 0;
   }

   /// @brief Tests emplacement and insertion of non-trivial element types.
   [[nodiscard]] int testEmplaceAndNonTrivialValues() {
      vve::v3::Vector<pair_value, 2> pairs{};
      const auto &back_pair = pairs.emplace_back(1, 2);
      if (back_pair != pair_value{1, 2}) {
         return 1;
      }

      const auto insert_it = pairs.emplace(pairs.cbegin(), 7, 8);
      if (insert_it == pairs.end() || *insert_it != pair_value{7, 8}) {
         return 2;
      }

      pairs.emplace_back(9, 10);
      if (pairs.size() != 3 || pairs[1] != pair_value{1, 2} || pairs[2] != pair_value{9, 10}) {
         return 3;
      }

      vve::v3::Vector<std::string, 3> names{};
      names.emplace_back("alpha");
      names.emplace_back(3, 'b');
      names.insert(names.cbegin() + 1, std::string{"middle"});
      if (names.size() != 3 || names[0] != "alpha" || names[1] != "middle" || names[2] != "bbb") {
         return 4;
      }

      return 0;
   }

   /// @brief Tests object lifetime accounting for erase, copy, move, and clear operations.
   [[nodiscard]] int testTrackedLifetimeAndOperations() {
      tracked_value::resetCounters();
      {
         vve::v3::Vector<tracked_value, 3> values{};
         values.emplace_back(1);
         values.emplace_back(2);
         values.emplace(values.cbegin() + 1, 3);

         if (tracked_value::alive_count != 3) {
            return 1;
         }

         if (values[0].value != 1 || values[1].value != 3 || values[2].value != 2) {
            return 2;
         }

         values.erase(values.cbegin());
         if (tracked_value::alive_count != 2 || values[0].value != 3 || values[1].value != 2) {
            return 3;
         }

         vve::v3::Vector<tracked_value, 3> copied(values);
         if (tracked_value::alive_count != 4 || copied.size() != 2) {
            return 4;
         }

         if (tracked_value::copy_count == 0) {
            return 5;
         }

         values.clear();
         if (tracked_value::alive_count != 2) {
            return 6;
         }
      }

      if (tracked_value::alive_count != 0) {
         return 7;
      }

      if (tracked_value::move_count == 0) {
         return 8;
      }

      return 0;
   }

} // namespace

/**
 * @brief Executes the segmented-vector regression tests.
 * @return Process exit code where each failing block returns a unique range.
 */
int main() {
   if (const auto result = testBasicGrowthAndAccess(); result != 0) {
      return 100 + result;
   }

   if (const auto result = testInsertEraseAndResize(); result != 0) {
      return 200 + result;
   }

   if (const auto result = testReserveAndConstruction(); result != 0) {
      return 300 + result;
   }

   if (const auto result = testIteratorsAndConstAccess(); result != 0) {
      return 400 + result;
   }

   if (const auto result = testCopyMoveSwapAndAssignment(); result != 0) {
      return 500 + result;
   }

   if (const auto result = testOutOfRangeAndConstAt(); result != 0) {
      return 600 + result;
   }

   if (const auto result = testEmplaceAndNonTrivialValues(); result != 0) {
      return 700 + result;
   }

   if (const auto result = testTrackedLifetimeAndOperations(); result != 0) {
      return 800 + result;
   }

   return 0;
}
