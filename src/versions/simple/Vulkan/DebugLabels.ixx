module;
#include <compare>
#include <vulkan/vulkan.h>

export module VVEngine.Simple.Vulkan:DebugLabels;
import std;

/**
	* @file
	* @brief Debug-build object names and pass labels that frame debuggers such as RenderDoc show.
	*
	* In Debug builds (NDEBUG undefined) the renderer names its G-buffer images, shadow maps, window attachments,
	* pipelines and command buffers through VK_EXT_debug_utils and wraps every pass in a labelled region, so RenderDoc's
	* resource list, texture viewer and event browser read "G-buffer 1 normal + metalness" or "Shadow: directional light 0
	* cascade 2" instead of numbered handles. Names and labels are also kept on the CPU for tests. In Release builds every
	* call does nothing.
	*
	* Functional objects:
	* - debugLabelColour: one RGBA colour per kind of pass for RenderDoc's event browser.
	* - VulkanDebugLabels: loads the VK_EXT_debug_utils entry points, names objects, and opens and closes labelled regions.
	*/
export namespace vve::simple {

	/// @brief Kinds of labelled regions; each gets its own colour in the event browser.
	enum class DebugLabelKind : std::uint8_t { shadow, geometry, lighting, forward, post, gui };

	/// @brief RGBA colour of a labelled region.
	[[nodiscard]] constexpr std::array<float, 4> debugLabelColour(DebugLabelKind kind) {
		switch (kind) {
		case DebugLabelKind::shadow: return {0.45F, 0.45F, 0.55F, 1.0F};
		case DebugLabelKind::geometry: return {0.20F, 0.55F, 0.95F, 1.0F};
		case DebugLabelKind::lighting: return {0.95F, 0.80F, 0.20F, 1.0F};
		case DebugLabelKind::forward: return {0.30F, 0.80F, 0.35F, 1.0F};
		case DebugLabelKind::post: return {0.85F, 0.40F, 0.85F, 1.0F};
		case DebugLabelKind::gui: return {0.90F, 0.50F, 0.25F, 1.0F};
		}
		return {1.0F, 1.0F, 1.0F, 1.0F};
	}

	/// @brief VK_EXT_debug_utils names and labels for the renderer's objects; inactive without the extension or in Release.
	struct VulkanDebugLabels {
		/// @brief True in Debug builds, where names and labels are recorded; false in Release, where calls do nothing.
		static constexpr bool compiled{
#ifndef NDEBUG
			true
#else
			false
#endif
		};

		/// @brief Loads the entry points when the instance enabled VK_EXT_debug_utils; names are still recorded for tests.
		void load(VkInstance instance, VkDevice device, bool extensionEnabled) {
			reset();
#ifndef NDEBUG
			device_ = device;
			if (!extensionEnabled || instance == VK_NULL_HANDLE) { return; }
			setName_ = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
			beginLabel_ = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
			endLabel_ = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
			if (setName_ == nullptr || beginLabel_ == nullptr || endLabel_ == nullptr) { setName_ = nullptr; beginLabel_ = nullptr; endLabel_ = nullptr; }
#else
			(void)instance;
			(void)device;
			(void)extensionEnabled;
#endif
		}

		/// @brief Forgets the device, entry points and recorded names.
		void reset() {
			setName_ = nullptr;
			beginLabel_ = nullptr;
			endLabel_ = nullptr;
			device_ = VK_NULL_HANDLE;
			names_.clear();
			labels_.clear();
			depth_ = 0U;
		}

		/// @brief Whether the names and labels reach Vulkan (and so RenderDoc), not only the CPU copy.
		[[nodiscard]] bool active() const { return setName_ != nullptr; }

		/// @brief Names a Vulkan object; a null handle is ignored.
		template <typename THandle> void name(VkObjectType type, THandle handle, std::string_view text) {
#ifndef NDEBUG
			const std::uint64_t value = handleValue(handle);
			if (value == 0U || device_ == VK_NULL_HANDLE) { return; }
			auto &stored = names_[value];
			stored = std::string{text};
			if (setName_ == nullptr) { return; }
			const VkDebugUtilsObjectNameInfoEXT info{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
				.objectType = type, .objectHandle = value, .pObjectName = stored.c_str()};
			(void)setName_(device_, &info);
#else
			(void)type;
			(void)handle;
			(void)text;
#endif
		}

		/// @brief Names an image and its views: the whole view gets the image name, layer views get "layer i" appended.
		template <typename TImage> void nameImage(const TImage &image, std::string_view text) {
			if constexpr (compiled) {
				name(VK_OBJECT_TYPE_IMAGE, image.image, text);
				name(VK_OBJECT_TYPE_IMAGE_VIEW, image.imageView, std::string{text} + " (view)");
				for (std::size_t layer{}; layer < image.layerViews.size(); ++layer) {
					name(VK_OBJECT_TYPE_IMAGE_VIEW, image.layerViews[layer], std::format("{} layer {}", text, layer));
				}
			} else {
				(void)image;
				(void)text;
			}
		}

		/// @brief Opens a labelled region in a command buffer; every begin needs a matching end in the same buffer.
		void begin(VkCommandBuffer commandBuffer, std::string_view text, DebugLabelKind kind) {
#ifndef NDEBUG
			labels_.push_back(std::string(2U * depth_, ' ') + std::string{text});
			++depth_;
			if (beginLabel_ == nullptr) { return; }
			const auto colour = debugLabelColour(kind);
			const std::string owned{text};
			const VkDebugUtilsLabelEXT label{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pLabelName = owned.c_str(),
				.color = {colour[0], colour[1], colour[2], colour[3]}};
			beginLabel_(commandBuffer, &label);
#else
			(void)commandBuffer;
			(void)text;
			(void)kind;
#endif
		}

		/// @brief Closes the innermost labelled region.
		void end(VkCommandBuffer commandBuffer) {
#ifndef NDEBUG
			if (depth_ > 0U) { --depth_; }
			if (endLabel_ != nullptr) { endLabel_(commandBuffer); }
#else
			(void)commandBuffer;
#endif
		}

		/// @brief Starts a new command buffer's label list; nesting is shown by two spaces per level.
		void startFrame() {
			labels_.clear();
			depth_ = 0U;
		}

		/// @brief Name given to an object, or empty (always empty in Release).
		template <typename THandle> [[nodiscard]] std::string nameOf(THandle handle) const {
			const auto found = names_.find(handleValue(handle));
			return found == names_.end() ? std::string{} : found->second;
		}

		/// @brief Labels opened in the last recorded command buffer, in order (always empty in Release).
		[[nodiscard]] const std::vector<std::string> &frameLabels() const { return labels_; }

	private:
		/// @brief Vulkan handles are pointers on 64-bit platforms and 64-bit integers elsewhere.
		template <typename THandle> [[nodiscard]] static std::uint64_t handleValue(THandle handle) {
			if constexpr (std::is_pointer_v<THandle>) { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle)); }
			else { return static_cast<std::uint64_t>(handle); }
		}

		PFN_vkSetDebugUtilsObjectNameEXT setName_{nullptr};	///< Null when the extension is off.
		PFN_vkCmdBeginDebugUtilsLabelEXT beginLabel_{nullptr};	///< Null when the extension is off.
		PFN_vkCmdEndDebugUtilsLabelEXT endLabel_{nullptr};		///< Null when the extension is off.
		VkDevice device_{VK_NULL_HANDLE};						///< Device owning the named objects.
		std::map<std::uint64_t, std::string> names_{};			///< Names by handle; also keeps the strings alive.
		std::vector<std::string> labels_{};					///< Labels of the last recorded command buffer.
		std::uint32_t depth_{};									///< Open labels in the current command buffer.
	};

} // namespace vve::simple
