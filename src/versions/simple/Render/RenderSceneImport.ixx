export module VEEngine.Simple:RenderSceneImport;
import std;
import :RenderSystem;

/**
	* @file
	* @brief Imported asset-scene conversion for the simple CPU render scene.
	*
	* Functional objects:
	* - RenderSystem import helpers read asset callbacks, compose node transforms, cache render meshes/materials, and instantiate imported scene objects.
	*/

namespace vve::simple::detail {

	/// @brief Composes a child TRS transform below a parent TRS transform during import.
	[[nodiscard]] auto composeTransform(Transform parent, Transform child) -> Transform {
		const auto child_origin = multiply(modelMatrix(parent), Vec4{child.translation.value.x, child.translation.value.y,
																	child.translation.value.z, one()});
		return Transform{.translation = Position{.value = Vec3{child_origin.x, child_origin.y, child_origin.z}},
							  .rotation = Rotation{.value = multiply(parent.rotation.value, child.rotation.value)},
							  .scale = Scale{.value = Vec3{parent.scale.value.x * child.scale.value.x,
																	 parent.scale.value.y * child.scale.value.y,
																	 parent.scale.value.z * child.scale.value.z}}};
	}

} // namespace vve::simple::detail

namespace vve::simple {

	/// @brief Returns imported node handles for a loaded asset scene without exposing catalog internals.
	auto RenderSystem::importedSceneNodes(SceneHandle scene) const											-> Vector<NodeHandle>{
		if (!imported_assets_.scene_nodes) { return {}; }
		const auto nodes = imported_assets_.scene_nodes(scene);
		return nodes ? *nodes : Vector<NodeHandle>{};
	}

	/// @brief Composes imported scene node transforms from root to leaves.
	auto RenderSystem::importedSceneWorldTransforms(SceneHandle scene) const
		-> Vector<std::tuple<NodeHandle, Transform, Mat4>>{
		if (!scene.valid() || !imported_assets_.scene_nodes || !imported_assets_.scene_root_node ||
			 !imported_assets_.scene_node_children || !imported_assets_.node_transform) {
			return {};
		}

		const auto nodes = imported_assets_.scene_nodes(scene);
		const auto root = imported_assets_.scene_root_node(scene);
		if (!nodes || nodes->empty() || !root) { return {}; }

		auto result = Vector<std::tuple<NodeHandle, Transform, Mat4>>{};
		auto pending = Vector<std::pair<NodeHandle, Transform>>{};
		result.reserve(nodes->size());

		const auto root_transform = imported_assets_.node_transform(*root);
		if (!root_transform) { return {}; }
		pending.push_back({*root, *root_transform});

		// Walk the asset hierarchy and compose each child below its parent world transform.
		while (!pending.empty()) {
			const auto [node, world_transform] = pending.back();
			pending.pop_back();
			if (std::ranges::find(*nodes, node) == nodes->end() ||
				 std::ranges::find(result, node, [](const auto &entry) { return std::get<0>(entry); }) != result.end()) {
				return {};
			}
			result.push_back({node, world_transform, detail::modelMatrix(world_transform)});

			const auto children = imported_assets_.scene_node_children(scene, node);
			if (!children) { return {}; }
			for (const auto child : *children) {
				const auto child_transform = imported_assets_.node_transform(child);
				if (!child_transform) { return {}; }
				pending.push_back({child, detail::composeTransform(world_transform, *child_transform)});
			}
		}
		return result.size() == nodes->size() ? result : Vector<std::tuple<NodeHandle, Transform, Mat4>>{};
	}

	/// @brief Lists imported mesh/material pairs with their node world transforms.
	auto RenderSystem::importedSceneMeshInstances(SceneHandle scene) const
		-> Vector<std::tuple<NodeHandle, MeshHandle, MaterialHandle, Transform, Mat4>>{
		if (!scene.valid() || !imported_assets_.node_meshes || !imported_assets_.mesh_material) { return {}; }
		const auto world_transforms = importedSceneWorldTransforms(scene);
		if (world_transforms.empty()) { return {}; }

		auto result = Vector<std::tuple<NodeHandle, MeshHandle, MaterialHandle, Transform, Mat4>>{};
		for (const auto &[node, world_transform, world] : world_transforms) {
			const auto meshes = imported_assets_.node_meshes(node);
			if (!meshes) { return {}; }
			for (const auto mesh : *meshes) {
				const auto material = imported_assets_.mesh_material(mesh);
				if (!material) { return {}; }
				result.emplace_back(node, mesh, *material, world_transform, world);
			}
		}
		return result;
	}

	/// @brief Reads imported mesh geometry through the public asset query callbacks.
	auto RenderSystem::importedMeshGeometry(MeshHandle mesh) const
		-> std::optional<std::tuple<Vector<Vec3>, Vector<Vec3>, Vector<Vec2>, Vector<std::uint32_t>>>{
		if (!mesh.valid() || !imported_assets_.mesh_positions || !imported_assets_.mesh_normals ||
			 !imported_assets_.mesh_texcoords || !imported_assets_.mesh_indices) {
			return std::nullopt;
		}

		const auto positions = imported_assets_.mesh_positions(mesh);
		const auto normals = imported_assets_.mesh_normals(mesh);
		const auto texcoords = imported_assets_.mesh_texcoords(mesh);
		const auto indices = imported_assets_.mesh_indices(mesh);
		if (!positions || positions->empty() || !normals || !texcoords || !indices) { return std::nullopt; }
		return std::tuple{*positions, *normals, *texcoords, *indices};
	}

	/// @brief Creates or reuses one render mesh for imported asset geometry.
	auto RenderSystem::acquireRenderMesh(MeshHandle imported_mesh)											-> std::optional<RenderMeshHandle>{
		const auto cached = imported_render_meshes_.find(imported_mesh);
		if (cached != imported_render_meshes_.end() && scene_.findMesh(cached->second) != nullptr) { return cached->second; }
		if (cached != imported_render_meshes_.end()) { imported_render_meshes_.erase(cached); }

		const auto geometry = importedMeshGeometry(imported_mesh);
		if (!geometry) { return std::nullopt; }

		const auto &[positions, normals, texcoords, indices] = *geometry;
		auto vertices = Vector<RenderVertex>{};
		vertices.reserve(positions.size());
		auto bounds = Bounds{.minimum = Position{.value = positions.front()},
									.maximum = Position{.value = positions.front()},
									.valid = true};

		// Convert asset vertex arrays into the same CPU render-vertex payload used by primitive meshes.
		for (std::size_t index{}; index < positions.size(); ++index) {
			const auto position = positions[index];
			bounds.minimum.value = Vec3{std::min(bounds.minimum.value.x, position.x),
												 std::min(bounds.minimum.value.y, position.y),
												 std::min(bounds.minimum.value.z, position.z)};
			bounds.maximum.value = Vec3{std::max(bounds.maximum.value.x, position.x),
												 std::max(bounds.maximum.value.y, position.y),
												 std::max(bounds.maximum.value.z, position.z)};
			vertices.push_back(RenderVertex{.position = position,
													 .normal = index < normals.size() ? normals[index] : RenderVertex{}.normal,
													 .uv = index < texcoords.size() ? texcoords[index] : RenderVertex{}.uv});
		}

		auto copied_indices = Vector<std::uint32_t>{};
		copied_indices.reserve(indices.size());
		for (const auto index : indices) { copied_indices.push_back(index); }
		const auto render_mesh = scene_.addMesh(std::move(vertices), std::move(copied_indices), bounds);
		imported_render_meshes_.emplace(imported_mesh, render_mesh);
		return render_mesh;
	}

	/// @brief Creates or reuses one render material for an imported asset material.
	auto RenderSystem::acquireRenderMaterial(MaterialHandle imported_material)							-> RenderMaterialHandle{
		const auto default_color = LinearColor{.value = oneVec3()};
		if (!imported_material.valid()) { return scene_.addMaterial(RenderMaterial{.base_color = default_color}); }

		const auto cached = imported_render_materials_.find(imported_material);
		if (cached != imported_render_materials_.end() && scene_.findMaterial(cached->second) != nullptr) {
			return cached->second;
		}
		if (cached != imported_render_materials_.end()) { imported_render_materials_.erase(cached); }

		const auto render_material = scene_.addMaterial(RenderMaterial{.base_color = default_color});
		imported_render_materials_.emplace(imported_material, render_material);
		return render_material;
	}

	/// @brief Reads imported material texture handles through the public asset query callback.
	auto RenderSystem::importedMaterialTextures(MaterialHandle material) const							-> std::optional<Vector<TextureHandle>>{
		if (!material.valid() || !imported_assets_.material_textures) { return std::nullopt; }
		const auto textures = imported_assets_.material_textures(material);
		if (!textures) { return std::nullopt; }
		return *textures;
	}

	/// @brief Creates an empty public scene-instance entry for a loaded scene.
	auto RenderSystem::instantiateScene(SceneHandle scene, SceneInstantiationOptions options)
		-> std::expected<RenderSceneInstanceHandle, Error>{
		if (!scene.valid() || importedSceneNodes(scene).empty()) { return std::unexpected(Error::missing_object); }
		const auto instance = RenderSceneInstanceHandle{RenderSceneInstanceHandle::counter_bit |
																	 (next_scene_instance_id_++ & RenderSceneInstanceHandle::id_mask)};
		auto &[_, objects] = *scene_instances_.emplace(instance, Vector<RenderObjectHandle>{}).first;
		scene_instance_sources_.emplace(instance, scene);

		// Geometry remains the default instantiation path.
		if (options.instantiate_geometry) {
			for (const auto &[node, mesh, material, world_transform, world] : importedSceneMeshInstances(scene)) {
				const auto render_mesh = acquireRenderMesh(mesh);
				if (!render_mesh) { continue; }
				const auto render_material = acquireRenderMaterial(material);
				auto render_instance = scene_.addInstance(*render_mesh, render_material, world_transform, world);
				if (!render_instance) { return std::unexpected(render_instance.error()); }
				const auto backend_index = appendBackendObject(*render_instance);
				if (!backend_index) { return std::unexpected(backend_index.error()); }
				forward().scene.objects[*backend_index].model = world;
				const auto object = registerRenderObject(*render_instance, *backend_index);
				objects.push_back(object);
				object_sources_.emplace(object, std::pair{instance, node});
			}
		}

		// Imported lights are opt-in and require the asset bridge callbacks.
		if (options.apply_lights && imported_assets_.scene_lights && imported_assets_.light_data) {
			const auto lights = imported_assets_.scene_lights(scene);
			if (!lights) { return std::unexpected(lights.error()); }
			for (const auto light : *lights) {
				const auto data = imported_assets_.light_data(light);
				if (!data) { return std::unexpected(data.error()); }
				switch (data->kind) {
				case LightKind::directional: addDirectionalLight(data->direction, data->color, data->intensity, {}); break;
				case LightKind::point: addPointLight(data->position, data->color, data->intensity, data->range); break;
				case LightKind::spot: addSpotLight(data->position, data->direction, data->color, data->intensity,
															 data->range, data->cone); break;
				}
			}
		}

		// Imported cameras are opt-in and require the asset bridge callbacks.
		if (options.apply_cameras && imported_assets_.scene_cameras && imported_assets_.camera_data) {
			const auto cameras = imported_assets_.scene_cameras(scene);
			if (!cameras) { return std::unexpected(cameras.error()); }
			for (const auto camera : *cameras) {
				const auto data = imported_assets_.camera_data(camera);
				if (!data) { return std::unexpected(data.error()); }
				scene_.addImportedCamera(*data);
			}
		}
		return instance;
	}

} // namespace vve::simple
