#if defined(VVE_ENGINE_IMPLEMENTATION_IS_V5)
import VEEngine.V5;
namespace ve = vve::v5;
#else
import VEEngine.V4;
namespace ve = vve::v4;
#endif

int main() {
   ve::ResourceSystem resources{};
   const auto mesh = resources.add(ve::ResourceKind::mesh, ve::ObjectName{.value = "triangle"});
   const auto texture = resources.add(ve::ResourceKind::texture, ve::ObjectName{.value = "albedo"});
   if (!mesh || !texture || !resources.contains(*mesh) || !resources.contains(*texture)) { return 1; }
   if (resources.resourceCount() != 2) { return 2; }

   const auto mesh_name = resources.resourceName(*mesh);
   const auto texture_kind = resources.resourceKind(*texture);
   if (!mesh_name || mesh_name->value != "triangle") { return 3; }
   if (!texture_kind || *texture_kind != ve::ResourceKind::texture) { return 4; }

   const auto missing = resources.resourceName(ve::ResourceHandle{});
   if (missing || missing.error() != ve::Error::missing_object) { return 5; }

   return 0;
}
