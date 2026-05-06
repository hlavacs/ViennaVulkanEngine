import VEEngine.V4;

int main() {
   vve::v4::ResourceSystem resources{};
   const auto mesh = resources.add(vve::v4::ResourceKind::mesh, vve::v4::ObjectName{.value = "triangle"});
   const auto texture = resources.add(vve::v4::ResourceKind::texture, vve::v4::ObjectName{.value = "albedo"});
   if (!mesh || !texture || !resources.contains(*mesh) || !resources.contains(*texture)) { return 1; }
   if (resources.resourceCount() != 2) { return 2; }

   const auto mesh_name = resources.resourceName(*mesh);
   const auto texture_kind = resources.resourceKind(*texture);
   if (!mesh_name || mesh_name->value != "triangle") { return 3; }
   if (!texture_kind || *texture_kind != vve::v4::ResourceKind::texture) { return 4; }

   const auto missing = resources.resourceName(vve::v4::ResourceHandle{});
   if (missing || missing.error() != vve::v4::Error::missing_object) { return 5; }

   return 0;
}
