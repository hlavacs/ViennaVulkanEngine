module;
#include <SDL3/SDL.h>
#include <imgui.h>
#include <vulkan/vulkan_core.h>
#if __has_include(<backends/imgui_impl_sdl3.h>)
#include <backends/imgui_impl_sdl3.h>
#else
#include <imgui_impl_sdl3.h>
#endif
#if __has_include(<backends/imgui_impl_vulkan.h>)
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_vulkan.h>
#endif

export module VEEngine.Simple:Gui;
import std;
export import :Types;

/// @file
/// @brief Tiny GUI descriptor table used until a real GUI backend is added.

namespace vve::simple {

	/// @brief Internal GUI widget record.
	struct GuiWidgetRecord {
		GuiWidgetHandle handle{};											///< Stable widget handle.
		std::string label{};													///< Text shown by the widget.
	};

} // namespace vve::simple

export namespace vve::simple {

	/// @brief Minimal GUI registry.
	class GuiSystem {
	public:
		auto draw(std::function<void()> frame)											-> void;
		auto initContext()																			-> void;
		auto initSDL(SDL_Window *window)													-> void;
		auto processEvent(const SDL_Event &event)								-> void;
		auto shutdownSDL()																		-> void;
		auto initVulkan(ImGui_ImplVulkan_InitInfo *info)					-> void;
		auto shutdownVulkan()																-> void;
		auto buildFonts()																		-> void;
		auto recordFrame(VkCommandBuffer cmd)									-> void;
		auto shutdownContext()																	-> void;
		[[nodiscard]] auto hasFrameCallback() const									-> bool;
		[[nodiscard]] auto label(std::string text)								-> std::expected<GuiWidgetHandle, Error>;
		[[nodiscard]] auto containsWidget(GuiWidgetHandle handle) const	-> bool;
		[[nodiscard]] auto widgetLabel(GuiWidgetHandle handle) const		-> std::expected<std::string, Error>;
		[[nodiscard]] auto widgetCount() const										-> std::size_t;

	private:
		std::map<GuiWidgetHandle, GuiWidgetRecord> widgets_{};	///< Widgets by handle.
		std::function<void()> frameCallback_{};						///< User frame callback stored for the future GUI backend.
		ImGuiContext *context_{nullptr};									///< Owned Dear ImGui context for this GUI system.
		bool sdlBackendReady_{false};										///< SDL backend lifecycle state.
		bool vulkanBackendReady_{false};								///< Vulkan backend lifecycle state.
		bool fontsReady_{false};												///< Vulkan font atlas lifecycle state.
	};

} // namespace vve::simple

export namespace vve::simple {

	/// @brief Stores the user callback that will build one immediate-mode GUI frame.
	inline auto GuiSystem::draw(std::function<void()> frame) -> void { frameCallback_ = std::move(frame); }

	/// @brief Creates the Dear ImGui context once for the simple GUI system.
	inline auto GuiSystem::initContext() -> void {
		if (!context_) { context_ = ImGui::CreateContext(); }
	}

	/// @brief Initializes the Dear ImGui SDL3 backend for a Vulkan window once.
	inline auto GuiSystem::initSDL(SDL_Window *window) -> void {
		if (!window || sdlBackendReady_) { return; }
		if (!context_) { initContext(); }
		ImGui_ImplSDL3_InitForVulkan(window);
		sdlBackendReady_ = true;
	}

	/// @brief Forwards one SDL event to Dear ImGui after SDL backend setup.
	inline auto GuiSystem::processEvent(const SDL_Event &event) -> void {
		if (!sdlBackendReady_) { return; }
		ImGui_ImplSDL3_ProcessEvent(&event);
	}

	/// @brief Shuts down the Dear ImGui SDL3 backend when it was initialized.
	inline auto GuiSystem::shutdownSDL() -> void {
		if (!sdlBackendReady_) { return; }
		ImGui_ImplSDL3_Shutdown();
		sdlBackendReady_ = false;
	}

	/// @brief Initializes the Dear ImGui Vulkan backend once with caller-provided engine data.
	inline auto GuiSystem::initVulkan(ImGui_ImplVulkan_InitInfo *info) -> void {
		if (!info || vulkanBackendReady_) { return; }
		if (!context_) { initContext(); }
		ImGui_ImplVulkan_Init(info);
		vulkanBackendReady_ = true;
	}

	/// @brief Shuts down the Dear ImGui Vulkan backend when it was initialized.
	inline auto GuiSystem::shutdownVulkan() -> void {
		if (!vulkanBackendReady_) { return; }
		ImGui_ImplVulkan_Shutdown();
		vulkanBackendReady_ = false;
		fontsReady_ = false;
	}

	/// @brief Uploads the Dear ImGui font atlas once after the Vulkan backend is ready.
	inline auto GuiSystem::buildFonts() -> void {
		if (!vulkanBackendReady_ || fontsReady_) { return; }
		ImGui_ImplVulkan_CreateFontsTexture();
		fontsReady_ = true;
	}

	/// @brief Records one Dear ImGui frame into the active Vulkan command buffer.
	inline auto GuiSystem::recordFrame(VkCommandBuffer cmd) -> void {
		if (!vulkanBackendReady_ || !fontsReady_) { return; }
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		if (frameCallback_) { frameCallback_(); }
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	}

	/// @brief Destroys the owned Dear ImGui context when it exists.
	inline auto GuiSystem::shutdownContext() -> void {
		if (context_) {
			ImGui::DestroyContext(context_);
			context_ = nullptr;
		}
	}

	/// @brief Returns whether a frame callback is currently stored.
	inline bool GuiSystem::hasFrameCallback() const { return static_cast<bool>(frameCallback_); }

	/// @brief Adds a text label and returns its handle.
	inline auto GuiSystem::label(std::string text)							-> std::expected<GuiWidgetHandle, Error>{
		const auto handle = makeCounterHandle<GuiWidgetHandle>();
		const auto [_, inserted] = widgets_.emplace(handle, GuiWidgetRecord{.handle = handle, .label = std::move(text)});
		if (!inserted) { return std::unexpected(Error::duplicate_object); }
		return handle;
	}

	/// @brief Returns whether a widget exists.
	inline bool GuiSystem::containsWidget(GuiWidgetHandle handle) const { return widgets_.contains(handle); }

	/// @brief Returns the label text for a widget.
	inline auto GuiSystem::widgetLabel(GuiWidgetHandle handle) const	-> std::expected<std::string, Error>{
		const auto widget = widgets_.find(handle);
		if (widget == widgets_.end()) { return std::unexpected(Error::missing_object); }
		return widget->second.label;
	}

	/// @brief Returns widget count.
	inline std::size_t GuiSystem::widgetCount() const { return widgets_.size(); }

} // namespace vve::simple
