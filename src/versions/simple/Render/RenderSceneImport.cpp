module VEEngine.Simple;
import std;

/**
	* @file
	* @brief Imported asset-scene conversion for the simple CPU render scene.
	*
	* Functional objects:
	* - RenderSystem import helpers read asset callbacks, compose node transforms, cache render meshes/materials, and instantiate imported scene objects.
	*/

namespace vve::simple::detail {

	/// @brief Splits a world matrix into translation, rotation, and scale.
	///
	/// Exact for matrices built from translation, rotation, and scale; shear (a rotated child under a non-uniformly
	/// scaled parent) cannot be expressed as a Transform and is dropped. The world matrix itself stays exact.
	[[nodiscard]] auto decomposeTransform(const Mat4 &matrix) -> Transform {
		auto axis_x = Vec3{matrix[0][0], matrix[0][1], matrix[0][2]};
		auto axis_y = Vec3{matrix[1][0], matrix[1][1], matrix[1][2]};
		auto axis_z = Vec3{matrix[2][0], matrix[2][1], matrix[2][2]};
		auto scale_factors = Vec3{length(axis_x), length(axis_y), length(axis_z)};
		if (dot(cross(axis_x, axis_y), axis_z) < zero()) { scale_factors.x = -scale_factors.x; } // Mirroring kept on one axis.
		const auto unit = [](Vec3 axis, Scalar factor) { return factor != zero() ? scale(axis, one() / factor) : axis; };
		axis_x = unit(axis_x, scale_factors.x);
		axis_y = unit(axis_y, scale_factors.y);
		axis_z = unit(axis_z, scale_factors.z);

		// Rotation matrix to quaternion (Shepperd); R(row, column) = axis_column[row].
		const Scalar trace = axis_x.x + axis_y.y + axis_z.z;
		auto rotation = identityQuat();
		if (trace > zero()) {
			const Scalar s = std::sqrt(trace + one()) * static_cast<Scalar>(2);
			rotation = Quat{s / static_cast<Scalar>(4), (axis_y.z - axis_z.y) / s, (axis_z.x - axis_x.z) / s, (axis_x.y - axis_y.x) / s};
		} else if (axis_x.x > axis_y.y && axis_x.x > axis_z.z) {
			const Scalar s = std::sqrt(one() + axis_x.x - axis_y.y - axis_z.z) * static_cast<Scalar>(2);
			rotation = Quat{(axis_y.z - axis_z.y) / s, s / static_cast<Scalar>(4), (axis_y.x + axis_x.y) / s, (axis_z.x + axis_x.z) / s};
		} else if (axis_y.y > axis_z.z) {
			const Scalar s = std::sqrt(one() + axis_y.y - axis_x.x - axis_z.z) * static_cast<Scalar>(2);
			rotation = Quat{(axis_z.x - axis_x.z) / s, (axis_y.x + axis_x.y) / s, s / static_cast<Scalar>(4), (axis_z.y + axis_y.z) / s};
		} else {
			const Scalar s = std::sqrt(one() + axis_z.z - axis_x.x - axis_y.y) * static_cast<Scalar>(2);
			rotation = Quat{(axis_x.y - axis_y.x) / s, (axis_z.x + axis_x.z) / s, (axis_z.y + axis_y.z) / s, s / static_cast<Scalar>(4)};
		}
		return Transform{.translation = Position{.value = Vec3{matrix[3][0], matrix[3][1], matrix[3][2]}},
							  .rotation = Rotation{.value = rotation},
							  .scale = Scale{.value = scale_factors}};
	}

} // namespace vve::simple::detail

namespace vve::simple {

	/// @brief Returns imported node handles for a loaded asset scene without exposing catalog internals.
	auto RenderSystem::importedSceneNodes(SceneHandle scene) const											-> Vector<NodeHandle>{
		if (!imported_assets_.scene_nodes) { return {}; }
		const auto nodes = imported_assets_.scene_nodes(scene);
		return nodes ? *nodes : Vector<NodeHandle>{};
	}

	/// @brief Composes imported scene node world matrices from root to leaves (parent world * child local).
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
		auto pending = Vector<std::pair<NodeHandle, Mat4>>{};
		result.reserve(nodes->size());

		const auto root_transform = imported_assets_.node_transform(*root);
		if (!root_transform) { return {}; }
		pending.push_back({*root, detail::modelMatrix(*root_transform)});

		// Walk the asset hierarchy; matrices compose correctly even under rotated, non-uniformly scaled parents.
		while (!pending.empty()) {
			const auto [node, world] = pending.back();
			pending.pop_back();
			if (std::ranges::find(*nodes, node) == nodes->end() ||
				 std::ranges::find(result, node, [](const auto &entry) { return std::get<0>(entry); }) != result.end()) {
				return {};
			}
			result.push_back({node, detail::decomposeTransform(world), world});

			const auto children = imported_assets_.scene_node_children(scene, node);
			if (!children) { return {}; }
			for (const auto child : *children) {
				const auto child_transform = imported_assets_.node_transform(child);
				if (!child_transform) { return {}; }
				pending.push_back({child, multiply(world, detail::modelMatrix(*child_transform))});
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
		-> std::optional<std::tuple<Vector<Vec3>, Vector<Vec3>, Vector<Vec2>, Vector<Vec4>, Vector<std::uint32_t>>>{
		if (!mesh.valid() || !imported_assets_.mesh_positions || !imported_assets_.mesh_normals ||
			 !imported_assets_.mesh_texcoords || !imported_assets_.mesh_tangents || !imported_assets_.mesh_indices) {
			return std::nullopt;
		}

		const auto positions = imported_assets_.mesh_positions(mesh);
		const auto normals = imported_assets_.mesh_normals(mesh);
		const auto texcoords = imported_assets_.mesh_texcoords(mesh);
		const auto tangents = imported_assets_.mesh_tangents(mesh);
		const auto indices = imported_assets_.mesh_indices(mesh);
		// A mesh made only of points or lines has no triangle indices left and nothing to draw.
		if (!positions || positions->empty() || !normals || !texcoords || !tangents || !indices || indices->empty()) { return std::nullopt; }
		return std::tuple{*positions, *normals, *texcoords, *tangents, *indices};
	}

	/// @brief Creates or reuses one render mesh for imported asset geometry.
	auto RenderSystem::acquireRenderMesh(MeshHandle imported_mesh)											-> std::optional<RenderMeshHandle>{
		const auto cached = imported_render_meshes_.find(imported_mesh);
		if (cached != imported_render_meshes_.end() && scene_.findMesh(cached->second) != nullptr) { return cached->second; }
		if (cached != imported_render_meshes_.end()) { imported_render_meshes_.erase(cached); }

		const auto geometry = importedMeshGeometry(imported_mesh);
		if (!geometry) { return std::nullopt; }

		const auto &[positions, normals, texcoords, tangents, indices] = *geometry;
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
											 .uv = index < texcoords.size() ? texcoords[index] : RenderVertex{}.uv,
											 .tangent = index < tangents.size() ? tangents[index] : RenderVertex{}.tangent});
		}

		auto copied_indices = Vector<std::uint32_t>{};
		copied_indices.reserve(indices.size());
		for (const auto index : indices) { copied_indices.push_back(index); }
		const auto render_mesh = scene_.addMesh(std::move(vertices), std::move(copied_indices), bounds);
		imported_render_meshes_.emplace(imported_mesh, render_mesh);
		return render_mesh;
	}

	/// @brief Creates or reuses one render material for an imported asset material.
	///
	/// Factors follow glTF: a texture multiplies its factor. Without a factor in the file, a texture alone defines the
	/// value (factor 1) and an untextured material gets the engine defaults (roughness 0.5, metalness 0, no emission).
	auto RenderSystem::acquireRenderMaterial(MaterialHandle imported_material)							-> RenderMaterialHandle{
		const auto default_color = LinearColor{.value = oneVec3()};
		if (!imported_material.valid()) {
			const auto material = scene_.addMaterial(RenderMaterial{.base_color = default_color});
			renderer_.markMaterialsDirty();
			return material;
		}

		const auto cached = imported_render_materials_.find(imported_material);
		if (cached != imported_render_materials_.end() && scene_.findMaterial(cached->second) != nullptr) {
			return cached->second;
		}
		if (cached != imported_render_materials_.end()) { imported_render_materials_.erase(cached); }

		auto material = RenderMaterial{.base_color = default_color};
		if (imported_assets_.material_base_color) {
			if (const auto color = imported_assets_.material_base_color(imported_material); color) {
				material.base_color = *color;
			}
		}
		auto height_texture = std::optional<RenderTextureIndex>{};
		if (imported_assets_.material_texture_sources) {
			if (const auto sources = imported_assets_.material_texture_sources(imported_material); sources) {
				for (const MaterialTextureSource &source : *sources) {
					const auto texture_index = scene_.acquireTexture(source.path, source.semantic, source.embedded.get());
					if (!texture_index) { continue; }
					switch (source.semantic) {
					case MaterialTextureSemantic::base_color:
						material.base_color_texture_index = *texture_index;
						material.base_color_texture = makeCounterHandle<TextureHandle>();
						if (const auto *texture = scene_.findTexture(*texture_index); texture != nullptr) {
							material.base_color_texture_source = texture->canonical_path;
						}
						break;
					case MaterialTextureSemantic::normal: material.normal_texture_index = *texture_index; break;
					case MaterialTextureSemantic::metalness: material.metalness_texture_index = *texture_index; break;
					case MaterialTextureSemantic::roughness: material.roughness_texture_index = *texture_index; break;
					case MaterialTextureSemantic::emissive: material.emissive_texture_index = *texture_index; break;
					case MaterialTextureSemantic::ambient_occlusion:
						material.ambient_occlusion_texture_index = *texture_index;
						break;
					case MaterialTextureSemantic::height: height_texture = *texture_index; break; // Only colored images get here.
					}
				}
			}
		}
		if (material.normal_texture_index == kNoRenderTexture && height_texture) { material.normal_texture_index = *height_texture; }

		auto factors = MaterialFactors{};
		if (imported_assets_.material_factors) {
			if (auto imported = imported_assets_.material_factors(imported_material); imported) { factors = *imported; }
		}
		const bool roughness_texture = material.roughness_texture_index != kNoRenderTexture;
		const bool metalness_texture = material.metalness_texture_index != kNoRenderTexture;
		const bool emissive_texture = material.emissive_texture_index != kNoRenderTexture;
		material.roughness = factors.roughness.value_or(roughness_texture ? one() : RenderMaterial{}.roughness);
		material.metalness = factors.metalness.value_or(metalness_texture ? one() : RenderMaterial{}.metalness);
		material.emissive = factors.emissive.value_or(LinearColor{.value = emissive_texture ? oneVec3() : zeroVec3()});
		// OBJ files always report an emissive color (black unless Ke is set); a black factor would hide an emissive map.
		if (emissive_texture && lengthSquared(material.emissive.value) == zero()) { material.emissive.value = oneVec3(); }

		const auto render_material = scene_.addMaterial(std::move(material));
		renderer_.markMaterialsDirty();
		imported_render_materials_.emplace(imported_material, render_material);
		return render_material;
	}

	/// @brief Instantiates an imported scene: objects, and optionally its lights and cameras.
	///
	/// Either the whole instance is created or, on an error, everything created so far is removed again.
	/// Meshes without vertices are skipped; they have nothing to draw.
	auto RenderSystem::instantiateScene(SceneHandle scene, SceneInstantiationOptions options)
		-> std::expected<RenderSceneInstanceHandle, Error>{
		if (!scene.valid() || importedSceneNodes(scene).empty()) { return std::unexpected(Error::missing_object); }
		const auto instance = RenderSceneInstanceHandle{RenderSceneInstanceHandle::counter_bit |
																	 (next_scene_instance_id_++ & RenderSceneInstanceHandle::id_mask)};
		scene_instances_.emplace(instance, Vector<RenderObjectHandle>{});
		scene_instance_sources_.emplace(instance, scene);
		const auto fail = [this, instance](Error error) -> std::expected<RenderSceneInstanceHandle, Error> {
			rollbackSceneInstance(instance);
			return std::unexpected(error);
		};

		if (options.instantiate_geometry) {
			for (const auto &[node, mesh, material, world_transform, world] : importedSceneMeshInstances(scene)) {
				const auto render_mesh = acquireRenderMesh(mesh);
				if (!render_mesh) { continue; }
				const auto render_material = acquireRenderMaterial(material);
				auto render_instance = scene_.addInstance(*render_mesh, render_material, world_transform, world);
				if (!render_instance) { return fail(render_instance.error()); }
				const auto object = registerRenderObject(*render_instance);
				scene_instances_[instance].push_back(object);
				object_sources_.emplace(object, std::pair{instance, node});
			}
		}

		// Imported lights are opt-in and require the asset bridge callbacks.
		if (options.apply_lights && imported_assets_.scene_lights && imported_assets_.light_data) {
			const auto lights = imported_assets_.scene_lights(scene);
			if (!lights) { return fail(lights.error()); }
			for (const auto light : *lights) {
				const auto data = imported_assets_.light_data(light);
				if (!data) { return fail(data.error()); }
				addImportedLight(*data, instance.value);
			}
		}

		// Imported cameras are opt-in and require the asset bridge callbacks.
		if (options.apply_cameras && imported_assets_.scene_cameras && imported_assets_.camera_data) {
			const auto cameras = imported_assets_.scene_cameras(scene);
			if (!cameras) { return fail(cameras.error()); }
			for (const auto camera : *cameras) {
				const auto data = imported_assets_.camera_data(camera);
				if (!data) { return fail(data.error()); }
				scene_.addImportedCamera(*data, instance.value);
			}
		}
		return instance;
	}

} // namespace vve::simple
