namespace vve::v3 {

   template <typename TECSImplementation = vve::detail::DefaultECSImplementation> class BasicWorldImplementation {
   public:
      using ecs_type = vve::ECSFacade<TECSImplementation>;

      explicit BasicWorldImplementation(ecs_type &ecs) noexcept : ecs_(&ecs) {}
      BasicWorldImplementation(ecs_type &ecs, const vve::detail::WorldRuntimeAccess &runtime_access) noexcept
          : ecs_(&ecs), runtime_access_(&runtime_access) {}

      [[nodiscard]] ecs_type &ecs() noexcept { return *ecs_; }
      [[nodiscard]] const ecs_type &ecs() const noexcept { return *ecs_; }

      [[nodiscard]] std::expected<vve::Handle, vve::Error> createEntity() { return ecs().create(); }
      [[nodiscard]] std::expected<vve::Handle, vve::Error> createObject() { return createEntity(); }
      [[nodiscard]] std::expected<bool, vve::Error> exists(vve::Handle entity) const { return ecs().exists(entity); }
      [[nodiscard]] std::expected<void, vve::Error> destroyEntity(vve::Handle entity) { return ecs().erase(entity); }
      [[nodiscard]] std::expected<void, vve::Error> destroyObject(vve::Handle entity) { return destroyEntity(entity); }
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
      [[nodiscard]] const vve::InputState &input() const {
         return runtime_access_ == nullptr || runtime_access_->input == nullptr ? vve::detail::emptyInputState()
                                                                                : *runtime_access_->input;
      }
      [[nodiscard]] std::expected<void, vve::Error> loadScene(const std::filesystem::path &path) {
         if (runtime_access_ == nullptr || runtime_access_->load_scene == nullptr) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         return runtime_access_->load_scene(runtime_access_->load_scene_context, path);
      }
      [[nodiscard]] std::expected<std::optional<vve::Transform>, vve::Error> getTransform(vve::Handle entity) const {
         return getComponent<vve::Transform>(entity);
      }
      [[nodiscard]] std::expected<void, vve::Error> setTransform(vve::Handle entity, const vve::Transform &transform) {
         return setComponent(entity, transform);
      }
      [[nodiscard]] std::expected<void, vve::Error> translate(vve::Handle entity, const vve::math::Vec3 &offset) {
         return modifyComponent<vve::Transform>(entity, [&offset](vve::Transform &transform) {
            transform.translation += offset;
         });
      }
      [[nodiscard]] std::expected<void, vve::Error> rotate(vve::Handle entity, const vve::math::Quat &rotation) {
         return modifyComponent<vve::Transform>(entity, [&rotation](vve::Transform &transform) {
            transform.rotation = vve::math::multiply(rotation, transform.rotation);
         });
      }
      [[nodiscard]] std::expected<void, vve::Error> setScale(vve::Handle entity, const vve::math::Vec3 &scale) {
         return modifyComponent<vve::Transform>(entity, [&scale](vve::Transform &transform) {
            transform.scale = scale;
         });
      }

      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<void, vve::Error> addComponent(vve::Handle entity, TComponent &&component) {
         return ecs().addComponent(entity, std::forward<TComponent>(component));
      }

      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<std::optional<std::remove_cvref_t<TComponent>>, vve::Error>
      getComponent(vve::Handle entity) const {
         return ecs().template get<TComponent>(entity);
      }

      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<void, vve::Error> setComponent(vve::Handle entity, TComponent &&component) {
         return ecs().put(entity, std::forward<TComponent>(component));
      }

      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<bool, vve::Error> hasComponent(vve::Handle entity) const {
         return ecs().template hasComponent<TComponent>(entity);
      }

      template <vve::NotHandle TComponent>
      [[nodiscard]] std::expected<void, vve::Error> removeComponent(vve::Handle entity) {
         return ecs().template eraseComponent<TComponent>(entity);
      }

      template <vve::NotHandle... TComponents>
         requires(sizeof...(TComponents) > 0)
      [[nodiscard]] std::expected<std::vector<vve::Handle>, vve::Error> view() const {
         return ecs().template view<std::remove_cvref_t<TComponents>...>();
      }

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

      template <vve::NotHandle TComponent, typename TMutator>
         requires(std::invocable<TMutator, std::remove_cvref_t<TComponent> &>)
      [[nodiscard]] std::expected<void, vve::Error> modifyComponent(vve::Handle entity, TMutator &&mutator) {
         auto component_result = getComponent<TComponent>(entity);
         if (!component_result) {
            return std::unexpected(component_result.error());
         }

         if (!component_result->has_value()) {
            return std::unexpected(vve::Error::invalid_argument);
         }

         auto component = **component_result;
         std::forward<TMutator>(mutator)(component);
         return setComponent(entity, std::move(component));
      }

   private:
      ecs_type *ecs_;
      const vve::detail::WorldRuntimeAccess *runtime_access_{nullptr};
   };

   using WorldImplementation = BasicWorldImplementation<>;

} // namespace vve::v3
