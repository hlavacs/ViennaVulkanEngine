module;
#include <cstdio>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
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

export module VVEngine.Simple:Gui;
import std;
export import VVEngine.Simple.Types;

/// @file
/// @brief Dear ImGui wrapper: one context, the SDL3 and Vulkan backends, and one user frame callback.

export namespace vve::simple {

	/// @brief Owns the Dear ImGui context and backend lifecycles for the simple engine.
	class GuiSystem {
	public:
		auto draw(std::function<void()> frame)											-> void;
		auto initContext(VkFormat colorFormat = VK_FORMAT_UNDEFINED)					-> void;
		auto initSDL(SDL_Window *window)													-> std::expected<void, Error>;
		auto processEvent(const SDL_Event &event)								-> bool;
		auto shutdownSDL()																		-> void;
		auto initVulkan(ImGui_ImplVulkan_InitInfo *info)					-> std::expected<void, Error>;
		auto shutdownVulkan()																-> void;
		auto buildFonts()																		-> std::expected<void, Error>;
		[[nodiscard]] auto prepareFrame()									-> bool;
		auto record(VkCommandBuffer cmd)										-> void;
		auto shutdownContext()																	-> void;
		[[nodiscard]] auto hasFrameCallback() const									-> bool;
		[[nodiscard]] bool ready() const;
		[[nodiscard]] bool takeFrameCallbackError();

	private:
		std::function<void()> frameCallback_{};						///< User frame callback stored for the future GUI backend.
		ImGuiContext *context_{nullptr};									///< Owned Dear ImGui context for this GUI system.
		bool sdlBackendReady_{false};										///< SDL backend lifecycle state.
		bool vulkanBackendReady_{false};								///< Vulkan backend lifecycle state.
		bool fontsReady_{false};												///< Vulkan font atlas lifecycle state.
		bool frameCallbackFailed_{false};									///< Callback error pending the next engine step.
	};

} // namespace vve::simple

export namespace vve::simple {

	/// @brief Stores the user callback that will build one immediate-mode GUI frame.
	inline auto GuiSystem::draw(std::function<void()> frame) -> void { frameCallback_ = std::move(frame); }

	/// @brief Creates the context once; an sRGB GUI target needs linear theme colours when mutable views are unavailable.
	inline auto GuiSystem::initContext(VkFormat colorFormat) -> void {
		if (!context_) {
			context_ = ImGui::CreateContext();
			ImGui::GetIO().IniFilename = nullptr; // Keep GUI layouts out of the working directory.
			if (colorFormat == VK_FORMAT_B8G8R8A8_SRGB || colorFormat == VK_FORMAT_R8G8B8A8_SRGB) {
				// Only the theme can be corrected here; user draw colours remain authored sRGB values.
				const auto linear = [](float value) {
					return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
				};
				for (auto &color : ImGui::GetStyle().Colors) {
					color.x = linear(color.x); color.y = linear(color.y); color.z = linear(color.z);
				}
			}
		}
	}

	/// @brief Initializes the SDL3 backend once; rejects null windows and reports backend failure.
	inline auto GuiSystem::initSDL(SDL_Window *window) -> std::expected<void, Error> {
		if (!window) { return std::unexpected(Error::invalid_argument); }
		if (sdlBackendReady_) { return {}; }
		if (!context_) { initContext(); }
		if (!ImGui_ImplSDL3_InitForVulkan(window)) { return std::unexpected(Error::platform_error); }
		sdlBackendReady_ = true;
		return {};
	}

	/// @brief Forwards one SDL event to Dear ImGui and reports whether the GUI claims it.
	///
	/// Keyboard events are claimed while an ImGui widget wants the keyboard (a focused text field), mouse events
	/// while the pointer is over a GUI window. The flags come from the previous ImGui frame, as ImGui intends.
	inline auto GuiSystem::processEvent(const SDL_Event &event) -> bool {
		if (!sdlBackendReady_) { return false; }
		ImGui_ImplSDL3_ProcessEvent(&event);
		const ImGuiIO &io = ImGui::GetIO();
		switch (event.type) {
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_TEXT_INPUT: return io.WantCaptureKeyboard;
		case SDL_EVENT_MOUSE_MOTION:
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_WHEEL: return io.WantCaptureMouse;
		default: return false;
		}
	}

	/// @brief Shuts down the Dear ImGui SDL3 backend when it was initialized.
	inline auto GuiSystem::shutdownSDL() -> void {
		if (!sdlBackendReady_) { return; }
		ImGui_ImplSDL3_Shutdown();
		sdlBackendReady_ = false;
	}

	/// @brief Initializes Vulkan after SDL3; reports invalid data, missing prerequisites or backend failure.
	inline auto GuiSystem::initVulkan(ImGui_ImplVulkan_InitInfo *info) -> std::expected<void, Error> {
		if (!info) { return std::unexpected(Error::invalid_argument); }
		if (!sdlBackendReady_) { return std::unexpected(Error::not_initialized); }
		if (vulkanBackendReady_) { return {}; }
		if (!ImGui_ImplVulkan_Init(info)) { return std::unexpected(Error::platform_error); }
		vulkanBackendReady_ = true;
		return {};
	}

	/// @brief Shuts down the Dear ImGui Vulkan backend when it was initialized.
	inline auto GuiSystem::shutdownVulkan() -> void {
		if (!vulkanBackendReady_) { return; }
		ImGui_ImplVulkan_Shutdown();
		vulkanBackendReady_ = false;
		fontsReady_ = false;
	}

	/// @brief Uploads fonts once; reports a missing Vulkan backend or an upload failure.
	inline auto GuiSystem::buildFonts() -> std::expected<void, Error> {
		if (!vulkanBackendReady_) { return std::unexpected(Error::not_initialized); }
		if (fontsReady_) { return {}; }
		if (!ImGui_ImplVulkan_CreateFontsTexture()) { return std::unexpected(Error::platform_error); }
		fontsReady_ = true;
		return {};
	}

	/// @brief Builds GUI draw data, recovering callback exceptions; reports whether any vertices need recording.
	inline auto GuiSystem::prepareFrame() -> bool {
		if (!ready()) { return false; }
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		// Preserve the GUI stacks so a callback exception can leave the frame safe to render.
		if (frameCallback_) {
			ImGuiErrorRecoveryState state{};
			ImGui::ErrorRecoveryStoreState(&state);
			try { frameCallback_(); }
			catch (const std::exception &error) {
				if (!frameCallbackFailed_) { std::println(stderr, "[vve::simple] GUI callback failed: {}", error.what()); }
				frameCallbackFailed_ = true;
				// Missing End/Pop calls are expected here; restore normal assertions after recovery.
				auto &io = ImGui::GetIO();
				const bool recovery_assert = std::exchange(io.ConfigErrorRecoveryEnableAssert, false);
				ImGui::ErrorRecoveryTryToRecoverState(&state);
				io.ConfigErrorRecoveryEnableAssert = recovery_assert;
			}
		}
		ImGui::Render();
		return ImGui::GetDrawData()->TotalVtxCount > 0;
	}

	/// @brief Records prepared draw data inside the GUI rendering pass without invoking the callback again.
	inline auto GuiSystem::record(VkCommandBuffer cmd) -> void {
		if (!ready()) { return; }
		auto *data = ImGui::GetDrawData();
		if (data && data->TotalVtxCount > 0) { ImGui_ImplVulkan_RenderDrawData(data, cmd); }
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

	/// @brief Reports whether the context, both backends and font atlas can record GUI frames.
	inline bool GuiSystem::ready() const { return context_ && sdlBackendReady_ && vulkanBackendReady_ && fontsReady_; }

	/// @brief Consumes a recovered callback error so the engine reports it exactly once.
	inline bool GuiSystem::takeFrameCallbackError() { return std::exchange(frameCallbackFailed_, false); }

} // namespace vve::simple
