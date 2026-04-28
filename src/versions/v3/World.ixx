/**
 * @file
 * @brief Concrete v3 world implementation behind the public world facade.
 */
namespace vve::v3 {

   /**
    * @brief Concrete v3 world implementation behind the public `vve::World` facade.
    *
    * This type binds ECS storage to optional runtime access services such as
    * window inspection, input state, and scene loading.
    */
   template <typename TECSImplementation = vve::detail::DefaultECSImplementation> class BasicWorldImplementation {
   public:
      using ecs_type = vve::ECSFacade<TECSImplementation>;

      /// @brief Creates a world implementation backed only by ECS storage.
      explicit BasicWorldImplementation(ecs_type &ecs) noexcept : ecs_(&ecs) {}
      /// @brief Creates a world implementation backed by ECS storage and runtime services.
      BasicWorldImplementation(ecs_type &ecs, const vve::detail::WorldRuntimeAccess &runtime_access) noexcept
          : ecs_(&ecs), runtime_access_(&runtime_access) {}

      /// @brief Returns mutable access to the backing ECS facade.
      [[nodiscard]] ecs_type &ecs() noexcept { return *ecs_; }
      /// @brief Returns read-only access to the backing ECS facade.
      [[nodiscard]] const ecs_type &ecs() const noexcept { return *ecs_; }

      /// @brief Creates a new entity.
      [[nodiscard]] std::expected<vve::Handle, vve::Error> createEntity() { return ecs().create(); }
      /// @brief Creates a new object entity. Currently equivalent to `createEntity()`.
      [[nodiscard]] std::expected<vve::Handle, vve::Error> createObject() { return createEntity(); }
      /// @brief Returns whether an entity currently exists.
      [[nodiscard]] std::expected<bool, vve::Error> exists(vve::Handle entity) const { return ecs().exists(entity); }
      /// @brief Destroys an entity and its components.
      [[nodiscard]] std::expected<void, vve::Error> destroyEntity(vve::Handle entity) { return ecs().erase(entity); }
      /// @brief Destroys an object entity. Currently equivalent to `destroyEntity()`.
      [[nodiscard]] std::expected<void, vve::Error> destroyObject(vve::Handle entity) { return destroyEntity(entity); }
      /// @brief Returns the current runtime-visible window range.
      [[nodiscard]] std::ranges::subrange<std::vector<vve::WindowInfo>::const_iterator> windows() const {
         if (runtime_access_ == nullptr || runtime_access_->windows_begin == runtime_access_->windows_end) {
            return {};
         }

         return std::ranges::subrange(runtime_access_->windows_begin, runtime_access_->windows_end);
      }
      [[nodiscard]] std::optional<vve::WindowInfo> findWindow(vve::Handle window) const {
         for (const auto &window_info : windows()) {
            if (window_info.handle == window) {
               return window_info;
            }
         }

         return std::nullopt;
      }
      [[nodiscard]] std::optional<vve::WindowInfo> findWindow(std::string_view window_id) const {
         for (const auto &window_info : windows()) {
            if (window_info.id == window_id) {
               return window_info;
            }
         }

         return std::nullopt;
      }
      /// @brief Returns the current input snapshot or an empty fallback when no runtime is bound.
      [[nodiscard]] const vve::InputState &input() const {
         return runtime_access_ == nullptr || runtime_access_->input == nullptr ? vve::detail::emptyInputState()
                                                                                : *runtime_access_->input;
      }
      /// @brief Forwards a scene-load request through the bound runtime callback.
      [[nodiscard]] std::expected<void, vve::Error> loadScene(const std::filesystem::path &path) {
         if (runtime_access_ == nullptr || runtime_access_->load_scene == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return runtime_access_->load_scene(runtime_access_->load_scene_context, path);
      }
      /// @brief Forwards an already imported scene through the bound runtime callback.
      template <typename TImportedScene>
      [[nodiscard]] std::expected<void, vve::Error> loadImportedScene(const TImportedScene &scene) {
         if (runtime_access_ == nullptr || runtime_access_->load_imported_scene == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return runtime_access_->load_imported_scene(runtime_access_->load_imported_scene_context,
                                                     std::addressof(scene));
      }
      /// @brief Forwards an active-camera update through the bound runtime callback.
      [[nodiscard]] std::expected<void, vve::Error> setCamera(const vve::Camera &camera) {
         if (runtime_access_ == nullptr || runtime_access_->set_camera == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return runtime_access_->set_camera(runtime_access_->set_camera_context, camera);
      }
      /// @brief Forwards a per-window active-camera update through the bound runtime callback.
      [[nodiscard]] std::expected<void, vve::Error> setCamera(std::string_view window_id,
                                                              const vve::Camera &camera) {
         if (window_id.empty()) {
            return setCamera(camera);
         }
         if (runtime_access_ == nullptr || runtime_access_->set_window_camera == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return runtime_access_->set_window_camera(runtime_access_->set_window_camera_context, window_id, camera);
      }
      /// @brief Returns the transform component when present.
      [[nodiscard]] std::expected<std::optional<vve::Transform>, vve::Error> getTransform(vve::Handle entity) const {
         return getComponent<vve::Transform>(entity);
      }
      /// @brief Replaces the transform component on an entity.
      [[nodiscard]] std::expected<void, vve::Error> setTransform(vve::Handle entity, const vve::Transform &transform) {
         return setComponent(entity, transform);
      }
      /// @brief Applies a translation offset to the transform component.
      [[nodiscard]] std::expected<void, vve::Error> translate(vve::Handle entity, const vve::math::Vec3 &offset) {
         return modifyComponent<vve::Transform>(entity, [&offset](vve::Transform &transform) {
            transform.translation += offset;
         });
      }
      /// @brief Applies a rotation to the transform component.
      [[nodiscard]] std::expected<void, vve::Error> rotate(vve::Handle entity, const vve::math::Quat &rotation) {
         return modifyComponent<vve::Transform>(entity, [&rotation](vve::Transform &transform) {
            transform.rotation = vve::math::multiply(rotation, transform.rotation);
         });
      }
      /// @brief Replaces the transform scale.
      [[nodiscard]] std::expected<void, vve::Error> setScale(vve::Handle entity, const vve::math::Vec3 &scale) {
         return modifyComponent<vve::Transform>(entity, [&scale](vve::Transform &transform) {
            transform.scale = scale;
         });
      }

      /// @brief Adds a component to an entity.
      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<void, vve::Error> addComponent(vve::Handle entity, TComponent &&component) {
         return ecs().addComponent(entity, std::forward<TComponent>(component));
      }

      /// @brief Returns a copy of a component when present.
      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, vve::Error>
      getComponent(vve::Handle entity) const {
         return ecs().template tryGet<TComponent>(entity);
      }

      /// @brief Replaces or inserts a component on an entity.
      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<void, vve::Error> setComponent(vve::Handle entity, TComponent &&component) {
         return ecs().put(entity, std::forward<TComponent>(component));
      }

      /// @brief Returns whether an entity owns a component of type `TComponent`.
      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<bool, vve::Error> hasComponent(vve::Handle entity) const {
         return ecs().template hasComponent<TComponent>(entity);
      }

      /// @brief Removes a component of type `TComponent` from an entity.
      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<void, vve::Error> removeComponent(vve::Handle entity) {
         return ecs().template eraseComponent<TComponent>(entity);
      }

      /// @brief Returns all entity handles that contain the requested component set.
      template <vve::NotHandle... TComponents>
         requires(sizeof...(TComponents) > 0)
      [[nodiscard]] std::expected<std::vector<vve::Handle>, vve::Error> view() const {
         return ecs().template view<std::remove_cvref_t<TComponents>...>();
      }

      /// @brief Creates an entity and attaches each supplied component in order.
      template <vve::NotHandle... TComponents>
      [[nodiscard]] std::expected<vve::Handle, vve::Error> spawn(TComponents &&...components) {
         const auto entity_result = createEntity();
         if (!entity_result) {
            return std::unexpected(entity_result.error());
         }

         const auto entity = *entity_result;
         std::expected<void, vve::Error> add_result{};
         (
             [&] {
                if (add_result) {
                   add_result = addComponent(entity, std::forward<TComponents>(components));
                }
             }(),
             ...);

         if (!add_result) {
            static_cast<void>(destroyEntity(entity));
            return std::unexpected(add_result.error());
         }

         return entity;
      }

      /// @brief Reads a component copy, mutates it, and writes it back.
      template <vve::NotHandle TComponent, typename TMutator>
         requires(std::invocable<TMutator, std::remove_cvref_t<TComponent> &>)
      [[nodiscard]] std::expected<void, vve::Error> modifyComponent(vve::Handle entity, TMutator &&mutator) {
         auto component_result = ecs().template get<TComponent>(entity);
         if (!component_result) {
            return std::unexpected(component_result.error());
         }

         auto component = *component_result;
         std::forward<TMutator>(mutator)(component);
         return setComponent(entity, std::move(component));
      }

   private:
      ecs_type *ecs_;
      const vve::detail::WorldRuntimeAccess *runtime_access_{nullptr};
   };

   /// @brief Default v3 world implementation used by the public facade.
   using WorldImplementation = BasicWorldImplementation<>;

} // namespace vve::v3
