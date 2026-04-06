export module VEEngine:Error;

/**
 * @file
 * @brief Common engine error codes used with `std::expected`.
 */
export namespace vve {

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

} // namespace vve
