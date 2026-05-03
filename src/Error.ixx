export module VEEngine:Error;
import std;

/**
 * @file
 * @brief Common engine error codes used with `std::expected`.
 */
export namespace vve {

   namespace detail {

      /// @brief Returns the mapped value or `fallback` when the key is unknown.
      template <typename TKey, typename TValue>
      [[nodiscard]] TValue mapValueOr(const std::map<TKey, TValue> &values, TKey key, TValue fallback) {
         if (const auto match = values.find(key); match != values.end()) {
            return match->second;
         }

         return fallback;
      }

   } // namespace detail

   /**
    * @brief Engine-wide coarse error classification.
    *
    * The public API uses this enum for lightweight error propagation across
    * subsystem facades without forcing exception-based control flow.
    */
   enum class Error {
      ok,                  ///< Operation completed successfully.
      not_initialized,     ///< Operation requires initialization that has not happened yet.
      already_initialized, ///< Operation attempted to initialize an already initialized object.
      invalid_argument,    ///< Caller supplied invalid input or violated an API precondition.
      file_not_found,      ///< Requested file could not be found.
      io_error,            ///< Generic I/O failure.
      unsupported_version, ///< Requested feature or version is not supported by the implementation.
      internal_error,      ///< Catch-all internal engine failure.
      invalid_handle,      ///< A handle was empty or unknown to the owner.
      duplicate_object,    ///< A descriptor map already contains the object.
      missing_object,      ///< A requested object descriptor does not exist.
      duplicate_component, ///< An entity already owns the component being added.
      missing_component,   ///< An entity does not own the requested component.
      platform_error,      ///< SDL or platform-window operation failed.
      asset_import_failed, ///< Asset import failed in the loader backend.
      cycle_detected       ///< A graph contains at least one dependency cycle.
   };

   /// @brief Returns the stable diagnostic name for an engine error code.
   [[nodiscard]] inline std::string_view errorName(Error error) {
      static const std::map<Error, std::string_view> names{
          {Error::ok, "ok"},
          {Error::not_initialized, "not_initialized"},
          {Error::already_initialized, "already_initialized"},
          {Error::invalid_argument, "invalid_argument"},
          {Error::file_not_found, "file_not_found"},
          {Error::io_error, "io_error"},
          {Error::unsupported_version, "unsupported_version"},
          {Error::internal_error, "internal_error"},
          {Error::invalid_handle, "invalid_handle"},
          {Error::duplicate_object, "duplicate_object"},
          {Error::missing_object, "missing_object"},
          {Error::duplicate_component, "duplicate_component"},
          {Error::missing_component, "missing_component"},
          {Error::platform_error, "platform_error"},
          {Error::asset_import_failed, "asset_import_failed"},
          {Error::cycle_detected, "cycle_detected"},
      };

      return detail::mapValueOr(names, error, std::string_view{"unknown_error"});
   }

} // namespace vve
