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
      not_initialized,     ///< Operation requires initialization that has not happened yet.
      already_initialized, ///< Operation attempted to initialize an already initialized object.
      invalid_argument,    ///< Caller supplied invalid input or violated an API precondition.
      file_not_found,      ///< Requested file could not be found.
      io_error,            ///< Generic I/O failure.
      unsupported_version, ///< Requested feature or version is not supported by the implementation.
      internal_error       ///< Catch-all internal engine failure.
   };

   /// @brief Returns the stable diagnostic name for an engine error code.
   [[nodiscard]] inline std::string_view errorName(Error error) {
      static const std::map<Error, std::string_view> names{
          {Error::not_initialized, "not_initialized"},
          {Error::already_initialized, "already_initialized"},
          {Error::invalid_argument, "invalid_argument"},
          {Error::file_not_found, "file_not_found"},
          {Error::io_error, "io_error"},
          {Error::unsupported_version, "unsupported_version"},
          {Error::internal_error, "internal_error"},
      };

      return detail::mapValueOr(names, error, std::string_view{"unknown_error"});
   }

} // namespace vve
