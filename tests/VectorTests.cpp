/// @file
/// @brief Checks contiguous facade sequences and the standard vector operations used by the engine.
import std;
import VVEngine;

/// @brief Verifies small allocations, contiguous handles, growth, insertion, erasure and range appending.
int main() {
   vve::Vector<vve::MeshHandle> handles{};
   handles.push_back(vve::MeshHandle{});
   if (handles.capacity() >= 256U) { return 8; }
   handles.push_back(vve::MeshHandle{});
   if (handles.data() + 1 != &handles[1]) { return 9; }
   static_assert(std::is_same_v<vve::Vector<vve::MeshHandle>, std::vector<vve::MeshHandle>>);
   std::println("VectorTests single_capacity_below_256=true contiguous=true");

   vve::Vector<int> values{};
   if (!values.empty() || values.capacity() != 0) { return 1; }
   // Exercise growth beyond the former container's minimum allocation.
   for (const auto value : std::views::iota(0, 260)) { values.push_back(value); }
   if (values.size() != 260U) { return 2; }
   if (values.front() != 0 || values.back() != 259) { return 3; }

   // Appending within reserved capacity keeps element references valid.
   values.reserve(values.size() + 1U);
   auto *stable = std::addressof(values[3]);
   values.push_back(777);
   if (std::addressof(values[3]) != stable || values.back() != 777) { return 4; }

   const auto inserted = values.insert(values.cbegin() + 1, 99);
   if (inserted == values.end() || *inserted != 99 || values[2] != 1) { return 5; }
   const auto erased = values.erase(values.cbegin() + 1);
   if (erased == values.end() || *erased != 1 || values[1] != 1) { return 6; }

   const auto tail = std::views::iota(0, 3);
   values.insert(values.end(), tail.begin(), tail.end());
   if (values.back() != 2) { return 7; }
   return 0;
}
