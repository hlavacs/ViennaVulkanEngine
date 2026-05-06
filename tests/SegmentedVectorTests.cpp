#include <ranges>

import VEEngine;

int main() {
   vve::Vector<int> values{};
   if (!values.empty() || values.capacity() != 0 || values.segmentCount() != 0) { return 1; }

   const auto segment_size = vve::Vector<int>::segmentSize();
   for (std::size_t index = 0; index < segment_size + 4; ++index) {
      values.push_back(static_cast<int>(index));
   }

   if (values.size() != segment_size + 4 || values.segmentCount() != 2) { return 2; }
   if (values.front() != 0 || values.back() != static_cast<int>(segment_size + 3)) { return 3; }

   auto *stable = std::addressof(values[3]);
   values.push_back(777);
   if (std::addressof(values[3]) != stable || values.back() != 777) { return 4; }

   const auto inserted = values.insert(values.cbegin() + 1, 99);
   if (inserted == values.end() || *inserted != 99 || values[2] != 1) { return 5; }

   const auto erased = values.erase(values.cbegin() + 1);
   if (erased == values.end() || *erased != 1 || values[1] != 1) { return 6; }

   values.appendRange(std::views::iota(0, 3));
   if (values.back() != 2) { return 7; }

   return 0;
}
