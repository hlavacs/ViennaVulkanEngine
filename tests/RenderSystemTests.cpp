import std;
import VEEngine.V4;

int main() {
   const vve::v4::RenderSystem render_system{};
   const auto renderer = render_system.createForwardRenderer();

   if (!renderer.handle.valid()) { return 1; }
   if (renderer.id.value != "forward") { return 2; }
   if (!renderer.shadow_maps) { return 3; }

   const auto second = render_system.createForwardRenderer();
   if (!second.handle.valid() || second.handle == renderer.handle) { return 4; }

   vve::v4::RenderScene scene{};
   const auto material = scene.addMaterial(vve::v4::RenderMaterial{
      .base_color = vve::v4::LinearColor{.value = vve::v4::Vec3(0.75F, 0.50F, 0.25F)},
      .base_color_texture_source = "albedo.png"});

   vve::v4::Vector<vve::v4::RenderVertex> vertices{};
   vertices.push_back(vve::v4::RenderVertex{.position = vve::v4::Vec3(0.0F, 0.0F, 0.0F)});
   vertices.push_back(vve::v4::RenderVertex{.position = vve::v4::Vec3(1.0F, 0.0F, 0.0F)});
   vertices.push_back(vve::v4::RenderVertex{.position = vve::v4::Vec3(0.0F, 1.0F, 0.0F)});

   vve::v4::Vector<std::uint32_t> indices{};
   indices.push_back(0);
   indices.push_back(1);
   indices.push_back(2);

   const auto mesh = scene.addMesh(std::move(vertices), std::move(indices),
                                   vve::v4::Bounds{.valid = true});
   const auto instance = scene.addInstance(mesh, material);
   if (!mesh.valid() || !material.valid() || !instance) { return 5; }
   if (scene.meshCount() != 1 || scene.materialCount() != 1 || scene.instanceCount() != 1) { return 6; }
   if (!scene.findMesh(mesh) || !scene.findMaterial(material) || !scene.findInstance(*instance)) { return 7; }

   scene.setCamera(vve::v4::RenderCamera{.target_extent = vve::v4::PixelExtent{.width = 640, .height = 480}});
   scene.setDirectionalLight(vve::v4::RenderDirectionalLight{});
   if (!scene.camera() || !scene.directionalLight()) { return 8; }
   if (scene.camera()->target_extent.width != 640) { return 9; }

   const auto missing = scene.addInstance(vve::v4::RenderMeshHandle{}, material);
   if (missing || missing.error() != vve::v4::Error::missing_object) { return 10; }

   return 0;
}
