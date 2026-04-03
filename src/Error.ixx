export module VEEngine:Error;

export namespace vve {

   enum class Error {
      not_initialized,
      already_initialized,
      invalid_argument,
      file_not_found,
      io_error,
      unsupported_version,
      internal_error
   };

} // namespace vve
