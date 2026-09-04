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
export import VEEngine.Simple.Types;

/// @file
/// @brief Dear ImGui wrapper: one context, the SDL3 and Vulkan backends, and one user frame callback.

export namespace vve::simple {

	/// @brief Owns the Dear ImGui context and backend lifecycles for the simple engine.
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

	private:
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

} // namespace vve::simple
