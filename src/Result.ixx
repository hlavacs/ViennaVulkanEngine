export module VEEngine:Result;

export namespace vve {

enum class Result {
    success = 0,
    not_initialized,
    already_initialized,
    invalid_argument,
    file_not_found,
    io_error,
    unsupported_version,
    internal_error
};

} // namespace vve
