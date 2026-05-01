export module VEEngine.V4:Vector;
import std;

/// @file
/// @brief Tiny v4 container alias module; most containers intentionally stay STL-backed.

export namespace vve::v4 {

   /// @brief Educational vector name used by v4 APIs while preserving ordinary STL behavior.
   template <typename T>
   using Vector = std::vector<T>;

} // namespace vve::v4
