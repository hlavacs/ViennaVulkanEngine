#include <cstdint>

import VEEngine;

int main() {
   using namespace vve;

   static_assert(sizeof(MeshHandle) == sizeof(std::uint64_t));

   constexpr auto counter = makeHandleForTest<MeshHandle>(42);
   static_assert(counter.valid());
   static_assert(counter.isCounter());
   static_assert(!counter.isSlotMapIndex());
   static_assert(counter.id() == 42);

   constexpr auto slot = makeSlotMapHandleForTest<NodeHandle>(9, 3);
   static_assert(slot.valid());
   static_assert(!slot.isCounter());
   static_assert(slot.isSlotMapIndex());
   static_assert(slot.slotIndex() == 9);
   static_assert(slot.generation() == 3);

   const auto runtime_a = makeCounterHandle<TextureHandle>();
   const auto runtime_b = makeCounterHandle<TextureHandle>();
   if (!runtime_a.valid() || !runtime_b.valid() || runtime_a == runtime_b) { return 1; }
   if (!runtime_a.isCounter() || !runtime_b.isCounter()) { return 2; }
   if (TextureHandle{}.valid()) { return 3; }

   return 0;
}
