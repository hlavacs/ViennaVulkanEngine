import VEEngine.V4;

int main() {
   const vve::v4::RendererFactory factory{};
   const auto renderer = factory.createForwardRenderer();

   if (!renderer.handle.valid()) { return 1; }
   if (renderer.id.value != "forward") { return 2; }
   if (!renderer.shadow_maps) { return 3; }

   const auto second = factory.createForwardRenderer();
   if (!second.handle.valid() || second.handle == renderer.handle) { return 4; }

   return 0;
}
