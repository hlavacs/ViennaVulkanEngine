export module VEEngine.V4:Error;
import std;

/// @file
/// @brief v4 error codes used with `std::expected`.
export namespace vve::v4 {

   namespace detail {

      template <typename TKey, typename TValue>
      [[nodiscard]] TValue mapValueOr(const std::map<TKey, TValue> &values, TKey key, TValue fallback) {
         if (const auto match = values.find(key); match != values.end()) { return match->second; }
         return fallback;
      }

   } // namespace detail

   enum class Error {
      ok,
      not_initialized,
      already_initialized,
      invalid_argument,
      file_not_found,
      io_error,
      unsupported_version,
      internal_error,
      invalid_handle,
      duplicate_object,
      missing_object,
      duplicate_component,
      missing_component,
      platform_error,
      asset_import_failed,
      cycle_detected
   };

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

} // namespace vve::v4
