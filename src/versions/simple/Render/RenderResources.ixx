module;

#include <stb_image.h>

export module VEEngine.Simple.RenderResources;
import std;
import VEEngine.Simple.Types;
import VEEngine.Simple.Scene;

/**
	* @file
	* @brief CPU render-resource model and storage for the simple render system.
	*
	* Functional objects:
	* - Render*Handle aliases identify CPU render resources and registry entries.
	* - RenderMesh, RenderMaterial, and RenderInstance store CPU scene geometry, material, and draw-item data.
	* - RenderScene owns the CPU render-resource storage and scene mutation helpers.
	*/

export namespace vve::simple {

	struct RenderMeshHandleTag {};																				///< simple render mesh handle tag.
	struct RenderMaterialHandleTag {};																			///< simple render material handle tag.
	struct RenderInstanceHandleTag {};																			///< simple render instance handle tag.

	using RenderMeshHandle	= TypedHandle<RenderMeshHandleTag>;										///< simple render mesh handle.
	using RenderMaterialHandle = TypedHandle<RenderMaterialHandleTag>;								///< simple render material handle.
	using RenderInstanceHandle = TypedHandle<RenderInstanceHandleTag>;								///< simple render instance handle.
	using RenderTextureIndex = std::uint32_t;															///< Dense index into the render-scene texture table.

	/// @brief CPU-side mesh resource.
	struct RenderMesh {
		RenderMeshHandle handle{};														///< Stable render mesh handle.
		std::shared_ptr<const std::vector<RenderVertex>> vertices{};		///< Shared source vertices; edits detach before writing.
		Vector<std::uint32_t> indices{};												///< Triangle indices.
	};

	/// @brief CPU-side material resource.
	struct RenderMaterial {
		RenderMaterialHandle handle{};												///< Stable render material handle.
		LinearColor base_color{.value = oneVec3()};								///< Base color factor.
		Scalar roughness{static_cast<Scalar>(0.5)};								///< Roughness factor; a roughness texture multiplies it.
		Scalar metalness{zero()};													///< Metalness factor; a metalness texture multiplies it.
		LinearColor emissive{.value = zeroVec3()};								///< Emissive factor; an emissive texture multiplies it.
		RenderTextureIndex base_color_texture_index{kNoTexture};			///< Base-color texture-table index.
		RenderTextureIndex normal_texture_index{kNoTexture};				///< Normal texture-table index.
		RenderTextureIndex metalness_texture_index{kNoTexture};			///< Metalness texture-table index.
		RenderTextureIndex roughness_texture_index{kNoTexture};			///< Roughness texture-table index.
		RenderTextureIndex emissive_texture_index{kNoTexture};			///< Emissive texture-table index.
		RenderTextureIndex ambient_occlusion_texture_index{kNoTexture}; ///< Ambient-occlusion texture-table index.
	};

	/// @brief Scene draw item connecting mesh, material, and transforms.
	struct RenderInstance {
		RenderInstanceHandle handle{};												///< Stable render instance handle.
		RenderMeshHandle mesh{};														///< Mesh drawn by this instance.
		RenderMaterialHandle material{};												///< Material used by this instance.
		Transform local_transform{};													///< Source scene local transform.
		Mat4 world_transform{identityMat4()};										///< World transform used by later renderers.
		bool visible{true};																///< True when this instance should be rendered.
		bool casts_shadow{true};													///< True when this instance participates in shadow passes.
		bool unlit{false};														///< True when this instance bypasses lighting.
	};

	/// @brief Minimal CPU scene that stores render resources but does not draw them.
	class RenderScene {
	public:
		[[nodiscard]] auto acquireTexture(const std::filesystem::path &source,
			MaterialTextureSemantic semantic = MaterialTextureSemantic::base_color, std::shared_ptr<const EmbeddedImage> embedded = {})
			-> std::expected<RenderTextureIndex, Error>;
		[[nodiscard]] auto textureForUpload(RenderTextureIndex index) -> std::expected<const RenderTexture *, Error>;
		template <typename Generation> void releaseUploadedPixels(Generation &&uploadedGeneration);
		[[nodiscard]] RenderMaterialHandle addMaterial(RenderMaterial material = {});
		[[nodiscard]] RenderMeshHandle addMesh(std::shared_ptr<const std::vector<RenderVertex>> vertices, Vector<std::uint32_t> indices);
		[[nodiscard]] RenderMeshHandle addTriangleMesh(Vector<Vec3> positions,
			Vector<std::uint32_t> indices);
		[[nodiscard]] auto addPlaneMesh(Vec2 half_extent)																		-> RenderMeshHandle;
		[[nodiscard]] auto addCuboidMesh(Vec3 minimum, Vec3 maximum)														-> RenderMeshHandle;
		[[nodiscard]] std::expected<RenderInstanceHandle, Error>
		addInstance(RenderMeshHandle mesh, RenderMaterialHandle material, Transform local = {},
						Mat4 world = identityMat4());
		auto setCamera()																							-> void;
		auto addImportedCamera(std::uint64_t owner = 0)										-> void;
		auto clear()																														-> void;
		auto eraseImportedBy(std::uint64_t owner)																				-> void;
		[[nodiscard]] auto eraseInstance(RenderInstanceHandle handle)													-> bool;
		auto eraseMesh(RenderMeshHandle handle) -> void;
		auto eraseMaterial(RenderMaterialHandle handle) -> void;
		auto releaseUnusedTextures() -> std::size_t;
		[[nodiscard]] auto purgeUnusedAssets()																				-> std::size_t;
		[[nodiscard]] RenderMesh *findMesh(RenderMeshHandle handle);
		[[nodiscard]] const RenderMesh *findMesh(RenderMeshHandle handle) const;
		[[nodiscard]] const RenderMaterial *findMaterial(RenderMaterialHandle handle) const;
		[[nodiscard]] const RenderTexture *findTexture(RenderTextureIndex index) const;
		[[nodiscard]] auto textureCount() const -> std::size_t;
		[[nodiscard]] auto textureDecodeCount() const -> std::size_t;
		[[nodiscard]] const std::deque<RenderTexture> &textures() const;
		[[nodiscard]] RenderInstance *findInstance(RenderInstanceHandle handle);
		[[nodiscard]] const RenderInstance *findInstance(RenderInstanceHandle handle) const;
		[[nodiscard]] auto meshCount() const																						-> std::size_t;
		[[nodiscard]] auto materialCount() const																					-> std::size_t;
		[[nodiscard]] auto instanceCount() const																					-> std::size_t;
		[[nodiscard]] auto vertexCount() const																						-> std::size_t;
		[[nodiscard]] auto indexCount() const																						-> std::size_t;
		[[nodiscard]] bool hasCamera() const;
		[[nodiscard]] std::size_t importedCameraCount() const;
		[[nodiscard]] const Vector<RenderMesh> &meshes() const;
		[[nodiscard]] const Vector<RenderInstance> &instances() const;
		[[nodiscard]] const Vector<RenderMaterial> &materials() const;

	private:
		static void appendFace(Vector<RenderVertex> &vertices, Vector<std::uint32_t> &indices,
										Vec3 normal, std::array<Vec3, 4> corners);

		Vector<RenderMesh> meshes_{};													///< CPU mesh resources.
		Vector<RenderMaterial> materials_{};										///< CPU material resources.
		Vector<RenderInstance> instances_{};										///< CPU draw items.
		std::unordered_map<RenderMeshHandle, std::size_t, HandleHash<RenderMeshHandle>> mesh_indices_{}; ///< Mesh handle to vector index.
		std::unordered_map<RenderMaterialHandle, std::size_t, HandleHash<RenderMaterialHandle>> material_indices_{}; ///< Material handle to vector index.
		std::unordered_map<RenderInstanceHandle, std::size_t, HandleHash<RenderInstanceHandle>> instance_indices_{}; ///< Instance handle to vector index.
		std::map<std::pair<std::filesystem::path, bool>, Error> texture_failures_{}; ///< Decode errors retained until clear().
		std::deque<RenderTexture> textures_{};									///< Texture table in shader-index order; released slots have generation 0 and are reused.
		std::map<std::pair<std::filesystem::path, bool>, RenderTextureIndex> texture_indices_{}; ///< (canonical path, linear) to texture-table index.
		std::size_t texture_decode_count_{}; ///< Decode attempts over this scene lifetime, including failures and across clear().
		std::uint64_t texture_generation_{0};										///< Last generation handed to a decoded texture.
		bool camera_{false};														///< Whether a default camera was explicitly set.
		std::vector<std::uint64_t> imported_cameras_{};						///< Owner tag per imported camera for counting and removal.
	};

} // namespace vve::simple

namespace vve::simple {
	/// @brief Returns a canonical absolute path when the filesystem can resolve the source.
	[[nodiscard]] inline auto canonicalTexturePath(const std::filesystem::path &source)
		-> std::expected<std::filesystem::path, Error> {
		if (source.empty()) { return std::unexpected(Error::io_error); }
		std::error_code error{};
		auto canonical = std::filesystem::weakly_canonical(source, error);
		if (error) {
			error.clear();
			canonical = std::filesystem::absolute(source, error).lexically_normal();
		}
		if (error || !canonical.is_absolute()) { return std::unexpected(Error::io_error); }
		return canonical;
	}

	/// @brief Reports whether every pixel is (nearly) grey, i.e. the image is a height map rather than a normal map.
	[[nodiscard]] inline auto isGreyscale(const std::vector<std::byte> &rgba8) -> bool {
		constexpr int tolerance{8}; // Leaves room for JPEG chroma noise.
		for (std::size_t offset{}; offset + 2U < rgba8.size(); offset += 4U) {
			const int red = std::to_integer<int>(rgba8[offset]);
			const int green = std::to_integer<int>(rgba8[offset + 1U]);
			const int blue = std::to_integer<int>(rgba8[offset + 2U]);
			if (std::abs(red - green) > tolerance || std::abs(green - blue) > tolerance) { return false; }
		}
		return true;
	}

	/// @brief Decodes one image into tight RGBA8 rows ordered bottom-up.
	///
	/// Assimp texture coordinates put v = 0 at the bottom of the image (OpenGL convention), while stb_image and
	/// Vulkan put row 0 at the top. Storing the rows bottom-up makes v = 0 sample the bottom row, so imported
	/// models and the procedural faces (v up) show their textures upright and tangent frames match normal maps.
	[[nodiscard]] inline auto decodeTexture(const std::filesystem::path &path, const EmbeddedImage *embedded)
		-> std::expected<RenderTexture, Error> {
		auto texture = RenderTexture{};
		if (embedded != nullptr && embedded->extent.width > 0U && embedded->extent.height > 0U) {
			const auto byte_count = static_cast<std::size_t>(embedded->extent.width) * embedded->extent.height * 4U;
			if (embedded->bytes.size() != byte_count) { return std::unexpected(Error::io_error); }
			texture.rgba8 = embedded->bytes;
			texture.extent = embedded->extent;
		} else {
			int width{};
			int height{};
			int channels{};
			const auto path_text = path.string();
			auto *decoded = embedded != nullptr
				? stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(embedded->bytes.data()),
					static_cast<int>(embedded->bytes.size()), &width, &height, &channels, STBI_rgb_alpha)
				: stbi_load(path_text.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			auto pixels = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>{decoded, stbi_image_free};
			if (!pixels || width <= 0 || height <= 0) { return std::unexpected(Error::io_error); }
			texture.extent = PixelExtent{.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height)};
			const auto *begin = reinterpret_cast<const std::byte *>(pixels.get());
			texture.rgba8.assign(begin, begin + static_cast<std::size_t>(texture.extent.width) * texture.extent.height * 4U);
		}

		const auto row_bytes = static_cast<std::size_t>(texture.extent.width) * 4U;
		for (std::size_t top{}, bottom{texture.extent.height - 1U}; top < bottom; ++top, --bottom) {
			std::swap_ranges(texture.rgba8.begin() + static_cast<std::ptrdiff_t>(top * row_bytes),
				texture.rgba8.begin() + static_cast<std::ptrdiff_t>((top + 1U) * row_bytes),
				texture.rgba8.begin() + static_cast<std::ptrdiff_t>(bottom * row_bytes));
		}
		texture.greyscale = isGreyscale(texture.rgba8);
		return texture;
	}

	/// @brief Finds or decodes one texture-table entry keyed by source and color space.
	///
	/// Color maps (base color, emissive) are sRGB and data maps are linear, so the same image used in both roles
	/// gets two entries. Released slots are reused; a full table is reported instead of silently dropping the texture.
	inline auto RenderScene::acquireTexture(const std::filesystem::path &source, MaterialTextureSemantic semantic,
		std::shared_ptr<const EmbeddedImage> embedded) -> std::expected<RenderTextureIndex, Error> {
		const bool linear = semantic != MaterialTextureSemantic::base_color && semantic != MaterialTextureSemantic::emissive;
		auto key = std::pair{source, linear};
		auto found = texture_indices_.find(key);
		auto failed = texture_failures_.find(key);
		// Canonical imported paths hit directly; resolve alternate file spellings only on a cache miss.
		if (found == texture_indices_.end() && failed == texture_failures_.end() && embedded == nullptr) {
			const auto canonical = canonicalTexturePath(source);
			if (!canonical) { return std::unexpected(canonical.error()); }
			key.first = *canonical;
			found = texture_indices_.find(key);
			failed = texture_failures_.find(key);
		}
		if (failed != texture_failures_.end()) { return std::unexpected(failed->second); }
		// A coloured HEIGHT image is a mislabeled normal map; cached hits need no pixel scan.
		const auto rejected_height = [semantic](const RenderTexture &texture) {
			return semantic == MaterialTextureSemantic::height && texture.greyscale;
		};
		if (found != texture_indices_.end()) {
			if (rejected_height(textures_[found->second])) { return std::unexpected(Error::invalid_argument); }
			return found->second;
		}

		const auto free_slot = std::ranges::find(textures_, std::uint64_t{0}, &RenderTexture::generation);
		if (free_slot == textures_.end() && textures_.size() >= kMaxSceneTextures) {
			return std::unexpected(Error::capacity_exceeded);
		}
		++texture_decode_count_;
		auto texture = decodeTexture(key.first, embedded.get());
		if (!texture) {
			texture_failures_.emplace(key, texture.error());
			return std::unexpected(texture.error());
		}
		if (rejected_height(*texture)) { return std::unexpected(Error::invalid_argument); }
		texture->canonical_path = key.first;
		texture->embedded = std::move(embedded);
		texture->linear = linear;
		texture->generation = ++texture_generation_;

		const auto index = static_cast<RenderTextureIndex>(std::distance(textures_.begin(), free_slot));
		if (free_slot != textures_.end()) { *free_slot = std::move(*texture); }
		else { textures_.push_back(std::move(*texture)); }
		texture_indices_.emplace(key, index);
		return index;
	}

	/// @brief Restores released pixels from the retained file/embedded source; a missing file returns Error::io_error.
	/// Renderer mutation is limited to this restoration and decode bookkeeping: extent, greyscale, attempt count and failure cache.
	inline auto RenderScene::textureForUpload(RenderTextureIndex index) -> std::expected<const RenderTexture *, Error> {
		if (findTexture(index) == nullptr) { return std::unexpected(Error::missing_object); }
		auto &texture = textures_[index];
		if (!texture.rgba8.empty()) { return &texture; }
		const auto key = std::pair{texture.canonical_path, texture.linear};
		if (const auto failed = texture_failures_.find(key); failed != texture_failures_.end()) {
			return std::unexpected(failed->second);
		}
		++texture_decode_count_;
		auto decoded = decodeTexture(texture.canonical_path, texture.embedded.get());
		if (!decoded) {
			texture_failures_.emplace(key, decoded.error());
			return std::unexpected(decoded.error());
		}
		// Keep slot identity and ownership while refreshing dimensions, classification and upload bytes.
		texture.rgba8 = std::move(decoded->rgba8);
		texture.extent = decoded->extent;
		texture.greyscale = decoded->greyscale;
		return &texture;
	}

	/// @brief RenderSystem frees decoded storage only when the same nonzero generation is resident on the GPU.
	/// The retained source lets textureForUpload() restore pixels; a missing file then returns Error::io_error.
	template <typename Generation> inline void RenderScene::releaseUploadedPixels(Generation &&uploadedGeneration) {
		// Slots pending upload or replacement must retain their pixels, even if an older generation is resident.
		for (const auto index : std::views::iota(std::size_t{}, textures_.size())) {
			auto &texture = textures_[index];
			if (texture.generation != 0U && texture.generation == uploadedGeneration(index)) {
				std::vector<std::byte>{}.swap(texture.rgba8);
			}
		}
	}

	/// @brief Adds one quad face to a CPU mesh.
	inline void RenderScene::appendFace(Vector<RenderVertex> &vertices, Vector<std::uint32_t> &indices,
													Vec3 normal, std::array<Vec3, 4> corners) {
		const auto base = static_cast<std::uint32_t>(vertices.size());
		const auto uvs = std::array{Vec2{0.0F, 0.0F}, Vec2{1.0F, 0.0F}, Vec2{1.0F, 1.0F}, Vec2{0.0F, 1.0F}};
		const auto tangent_xyz = normalize(subtract(corners[1], corners[0]));
		const auto bitangent = normalize(subtract(corners[3], corners[0]));
		const auto tangent = Vec4{tangent_xyz.x, tangent_xyz.y, tangent_xyz.z,
			dot(cross(normal, tangent_xyz), bitangent) < zero() ? -one() : one()};
		for (std::size_t corner_index{}; corner_index < corners.size(); ++corner_index) {
			vertices.push_back(RenderVertex{.position = corners[corner_index], .normal = normal,
				.uv = uvs[corner_index], .tangent = tangent});
		}
		for (const auto index : std::array{0U, 1U, 2U, 0U, 2U, 3U}) { indices.push_back(base + index); }
	}

	/// @brief Registers a material and returns its handle.
	inline auto RenderScene::addMaterial(RenderMaterial material)											-> RenderMaterialHandle{
		material.handle = material.handle.valid() ? material.handle : makeCounterHandle<RenderMaterialHandle>();
		materials_.push_back(std::move(material));
		material_indices_.try_emplace(materials_.back().handle, materials_.size() - 1U);
		return materials_.back().handle;
	}

	/// @brief Registers a mesh and returns its handle.
	inline RenderMeshHandle RenderScene::addMesh(std::shared_ptr<const std::vector<RenderVertex>> vertices, Vector<std::uint32_t> indices) {
		auto mesh = RenderMesh{.handle = makeCounterHandle<RenderMeshHandle>(),
										.vertices = std::move(vertices),
										.indices = std::move(indices)};
		meshes_.push_back(std::move(mesh));
		mesh_indices_.try_emplace(meshes_.back().handle, meshes_.size() - 1U);
		return meshes_.back().handle;
	}

	/// @brief Converts public positions into one CPU-side indexed triangle mesh; zero normals make the shader use face normals.
	inline RenderMeshHandle RenderScene::addTriangleMesh(
		Vector<Vec3> positions, Vector<std::uint32_t> indices) {
		auto vertices = Vector<RenderVertex>{};
		vertices.reserve(positions.size());
		for (const Vec3 &position : positions) {
			vertices.push_back(RenderVertex{.position = position});
		}
		return addMesh(std::make_shared<std::vector<RenderVertex>>(std::move(vertices)), std::move(indices));
	}

	/// @brief Creates a two-triangle plane mesh.
	inline auto RenderScene::addPlaneMesh(Vec2 half_extent)													-> RenderMeshHandle{
		auto vertices = Vector<RenderVertex>{};
		auto indices = Vector<std::uint32_t>{};
		appendFace(vertices, indices, Vec3{0.0F, 1.0F, 0.0F},
						{Vec3{-half_extent.x, 0.0F, -half_extent.y}, Vec3{-half_extent.x, 0.0F, half_extent.y},
						Vec3{half_extent.x, 0.0F, half_extent.y}, Vec3{half_extent.x, 0.0F, -half_extent.y}});
		return addMesh(std::make_shared<std::vector<RenderVertex>>(std::move(vertices)), std::move(indices));
	}

	/// @brief Creates a six-face cuboid mesh.
	inline auto RenderScene::addCuboidMesh(Vec3 minimum, Vec3 maximum)									-> RenderMeshHandle{
		auto vertices = Vector<RenderVertex>{};
		auto indices = Vector<std::uint32_t>{};
		appendFace(vertices, indices, Vec3{0.0F, 0.0F, 1.0F},
						{Vec3{minimum.x, minimum.y, maximum.z}, Vec3{maximum.x, minimum.y, maximum.z},
						Vec3{maximum.x, maximum.y, maximum.z}, Vec3{minimum.x, maximum.y, maximum.z}});
		appendFace(vertices, indices, Vec3{0.0F, 0.0F, -1.0F},
						{Vec3{maximum.x, minimum.y, minimum.z}, Vec3{minimum.x, minimum.y, minimum.z},
						Vec3{minimum.x, maximum.y, minimum.z}, Vec3{maximum.x, maximum.y, minimum.z}});
		appendFace(vertices, indices, Vec3{1.0F, 0.0F, 0.0F},
						{Vec3{maximum.x, minimum.y, maximum.z}, Vec3{maximum.x, minimum.y, minimum.z},
						Vec3{maximum.x, maximum.y, minimum.z}, Vec3{maximum.x, maximum.y, maximum.z}});
		appendFace(vertices, indices, Vec3{-1.0F, 0.0F, 0.0F},
						{Vec3{minimum.x, minimum.y, minimum.z}, Vec3{minimum.x, minimum.y, maximum.z},
						Vec3{minimum.x, maximum.y, maximum.z}, Vec3{minimum.x, maximum.y, minimum.z}});
		appendFace(vertices, indices, Vec3{0.0F, 1.0F, 0.0F},
						{Vec3{minimum.x, maximum.y, maximum.z}, Vec3{maximum.x, maximum.y, maximum.z},
						Vec3{maximum.x, maximum.y, minimum.z}, Vec3{minimum.x, maximum.y, minimum.z}});
		appendFace(vertices, indices, Vec3{0.0F, -1.0F, 0.0F},
						{Vec3{minimum.x, minimum.y, minimum.z}, Vec3{maximum.x, minimum.y, minimum.z},
						Vec3{maximum.x, minimum.y, maximum.z}, Vec3{minimum.x, minimum.y, maximum.z}});
		return addMesh(std::make_shared<std::vector<RenderVertex>>(std::move(vertices)), std::move(indices));
	}

	/// @brief Registers an instance when its mesh and material exist.
	inline std::expected<RenderInstanceHandle, Error>
	RenderScene::addInstance(RenderMeshHandle mesh, RenderMaterialHandle material, Transform local, Mat4 world) {
		if (findMesh(mesh) == nullptr || findMaterial(material) == nullptr) { return std::unexpected(Error::missing_object); }
		auto instance = RenderInstance{.handle = makeCounterHandle<RenderInstanceHandle>(),
													.mesh = mesh, .material = material,
													.local_transform = local, .world_transform = world};
		instances_.push_back(std::move(instance));
		instance_indices_.try_emplace(instances_.back().handle, instances_.size() - 1U);
		return instances_.back().handle;
	}

	/// @brief Records that the renderer has an explicitly set default camera.
	inline void RenderScene::setCamera() { camera_ = true; }

	/// @brief Retains one imported camera owner for scene counts and per-instance removal.
	inline void RenderScene::addImportedCamera(std::uint64_t owner) { imported_cameras_.push_back(owner); }

	/// @brief Removes all scene resources; texture generations keep counting so the renderer sees every slot change.
	inline auto RenderScene::clear()																					-> void{
		meshes_.clear();
		materials_.clear();
		instances_.clear();
		textures_.clear();
		texture_indices_.clear();
		texture_failures_.clear();
		mesh_indices_.clear();
		material_indices_.clear();
		instance_indices_.clear();
		camera_ = false;
		imported_cameras_.clear();
	}

	/// @brief Removes the cameras one scene instance imported.
	inline auto RenderScene::eraseImportedBy(std::uint64_t owner)												-> void{
		std::erase(imported_cameras_, owner);
	}

	namespace detail {
		/// @brief Restores handle indices after stable vector compaction.
		inline void rebuildResourceIndices(const auto &resources, auto &indices) {
			indices.clear();
			indices.reserve(resources.size());
			std::size_t index{};
			for (const auto &resource : resources) { indices.try_emplace(resource.handle, index++); }
		}

		/// @brief Removes matching resources without changing the order of surviving handles.
		inline bool eraseResource(auto &resources, auto &indices, auto handle) {
			if (!indices.contains(handle)) { return false; }
			std::erase_if(resources, [handle](const auto &resource) { return resource.handle == handle; });
			rebuildResourceIndices(resources, indices);
			return true;
		}
	} // namespace detail

	/// @brief Removes one CPU draw item by stable handle.
	inline auto RenderScene::eraseInstance(RenderInstanceHandle handle)										-> bool{
		return detail::eraseResource(instances_, instance_indices_, handle);
	}

	/// @brief Removes one mesh after the coordinator has checked its ownership and references.
	inline auto RenderScene::eraseMesh(RenderMeshHandle handle) -> void {
		(void)detail::eraseResource(meshes_, mesh_indices_, handle);
	}

	/// @brief Removes one material after the coordinator has checked its ownership and references.
	inline auto RenderScene::eraseMaterial(RenderMaterialHandle handle) -> void {
		(void)detail::eraseResource(materials_, material_indices_, handle);
	}

	/// @brief Removes mesh and material resources no live instance references.
	inline auto RenderScene::purgeUnusedAssets()																-> std::size_t{
		auto used_meshes = std::unordered_set<RenderMeshHandle, HandleHash<RenderMeshHandle>>{};
		auto used_materials = std::unordered_set<RenderMaterialHandle, HandleHash<RenderMaterialHandle>>{};
		used_meshes.reserve(meshes_.size());
		used_materials.reserve(materials_.size());
		// Collect both reference sets in one pass, then compact each resource vector once.
		for (const auto &instance : instances_) {
			used_meshes.insert(instance.mesh);
			used_materials.insert(instance.material);
		}
		const auto mesh_count = meshes_.size();
		const auto material_count = materials_.size();
		meshes_.erase(std::remove_if(meshes_.begin(), meshes_.end(),
			[&](const RenderMesh &mesh) { return !used_meshes.contains(mesh.handle); }), meshes_.end());
		materials_.erase(std::remove_if(materials_.begin(), materials_.end(),
			[&](const RenderMaterial &material) { return !used_materials.contains(material.handle); }), materials_.end());
		detail::rebuildResourceIndices(meshes_, mesh_indices_);
		detail::rebuildResourceIndices(materials_, material_indices_);
		releaseUnusedTextures();
		return (mesh_count - meshes_.size()) + (material_count - materials_.size());
	}

	/// @brief Frees texture slots no material references; later acquisitions reuse them.
	inline auto RenderScene::releaseUnusedTextures()														-> std::size_t{
		auto used = std::vector<bool>(textures_.size(), false);
		for (const RenderMaterial &material : materials_) {
			for (const RenderTextureIndex index : {material.base_color_texture_index, material.normal_texture_index,
					material.metalness_texture_index, material.roughness_texture_index, material.emissive_texture_index,
					material.ambient_occlusion_texture_index}) {
				if (index < used.size()) { used[index] = true; }
			}
		}
		std::size_t released{};
		for (std::size_t index{}; index < textures_.size(); ++index) {
			RenderTexture &texture = textures_[index];
			if (texture.generation == 0U || used[index]) { continue; }
			texture_indices_.erase(std::pair{texture.canonical_path, texture.linear});
			texture = RenderTexture{};
			++released;
		}
		while (!textures_.empty() && textures_.back().generation == 0U) { textures_.pop_back(); }
		return released;
	}

	/// @brief Finds a mesh by handle.
	inline RenderMesh *RenderScene::findMesh(RenderMeshHandle handle) {
		const auto found = mesh_indices_.find(handle);
		return found == mesh_indices_.end() ? nullptr : std::addressof(meshes_[found->second]);
	}

	/// @brief Finds a mesh by handle.
	inline const RenderMesh *RenderScene::findMesh(RenderMeshHandle handle) const {
		const auto found = mesh_indices_.find(handle);
		return found == mesh_indices_.end() ? nullptr : std::addressof(meshes_[found->second]);
	}

	/// @brief Finds a material by handle.
	inline const RenderMaterial *RenderScene::findMaterial(RenderMaterialHandle handle) const {
		const auto found = material_indices_.find(handle);
		return found == material_indices_.end() ? nullptr : std::addressof(materials_[found->second]);
	}

	/// @brief Counts image decode attempts, including failures; clearing the scene keeps the diagnostic cumulative.
	inline std::size_t RenderScene::textureDecodeCount() const { return texture_decode_count_; }

	/// @brief Finds a decoded texture by its table index; released slots are reported as missing.
	inline const RenderTexture *RenderScene::findTexture(RenderTextureIndex index) const {
		return index < textures_.size() && textures_[index].generation != 0U ? std::addressof(textures_[index]) : nullptr;
	}

	/// @brief Finds an instance by handle.
	inline RenderInstance *RenderScene::findInstance(RenderInstanceHandle handle) {
		const auto found = instance_indices_.find(handle);
		return found == instance_indices_.end() ? nullptr : std::addressof(instances_[found->second]);
	}

	/// @brief Finds an instance by handle.
	inline const RenderInstance *RenderScene::findInstance(RenderInstanceHandle handle) const {
		const auto found = instance_indices_.find(handle);
		return found == instance_indices_.end() ? nullptr : std::addressof(instances_[found->second]);
	}

	inline std::size_t RenderScene::meshCount() const { return meshes_.size(); }
	inline std::size_t RenderScene::materialCount() const { return materials_.size(); }
	inline std::size_t RenderScene::textureCount() const {
		return static_cast<std::size_t>(std::ranges::count_if(textures_, [](const RenderTexture &texture) { return texture.generation != 0U; }));
	}
	inline const std::deque<RenderTexture> &RenderScene::textures() const { return textures_; }
	inline std::size_t RenderScene::instanceCount() const { return instances_.size(); }

	/// @brief Returns the total source vertex count.
	inline auto RenderScene::vertexCount() const																	-> std::size_t{
		return std::accumulate(meshes_.begin(), meshes_.end(), std::size_t{}, [](std::size_t total, const auto &mesh) {
			return total + mesh.vertices->size();
		});
	}

	/// @brief Returns the total source index count.
	inline auto RenderScene::indexCount() const																	-> std::size_t{
		return std::accumulate(meshes_.begin(), meshes_.end(), std::size_t{}, [](std::size_t total, const auto &mesh) {
			return total + mesh.indices.size();
		});
	}

	/// @brief Returns whether a default camera was explicitly set.
	inline bool RenderScene::hasCamera() const { return camera_; }
	/// @brief Counts imported cameras without retaining duplicate descriptors.
	inline std::size_t RenderScene::importedCameraCount() const { return imported_cameras_.size(); }
	inline const Vector<RenderMesh> &RenderScene::meshes() const { return meshes_; }
	inline const Vector<RenderInstance> &RenderScene::instances() const { return instances_; }
	inline const Vector<RenderMaterial> &RenderScene::materials() const { return materials_; }

} // namespace vve::simple
