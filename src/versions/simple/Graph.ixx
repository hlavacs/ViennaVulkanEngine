export module VEEngine.Simple:Graph;
import std;
export import VEEngine.Simple.Types;

/// @file
/// @brief Generic named DAG and tree helpers used by the simple asset scene tree.

export namespace vve::simple {

	/// @brief Small named DAG with reverse edges for parent lookup.
	template <typename THandle> class Graph {
	public:
		[[nodiscard]] std::expected<void, Error> addNode(THandle handle, ObjectName name = {});
		auto addEdge(THandle from, THandle to)														-> void;
		[[nodiscard]] auto contains(THandle handle) const										-> bool;
		[[nodiscard]] auto children(THandle handle) const										-> Vector<THandle>;
		[[nodiscard]] auto parents(THandle handle) const										-> Vector<THandle>;

	private:
		/// @brief Adjacency storage allowing several neighbors per node handle.
		using EdgeMap = std::unordered_multimap<THandle, THandle, HandleHash<THandle>>;


		std::map<THandle, ObjectName> nodes_{};	///< Node labels by handle.
		EdgeMap outgoing_{};								///< Forward dependency edges.
		EdgeMap incoming_{};								///< Reverse dependency edges.
	};

	/// @brief Tree view over the generic graph, keeping only one root handle as tree-specific state.
	template <typename THandle> class Tree : public Graph<THandle> {
	private:
		/// @brief Reused graph storage and traversal implementation.
		using Base = Graph<THandle>;

	public:
		[[nodiscard]] std::expected<void, Error> setRoot(THandle handle, ObjectName name = {});
		[[nodiscard]] std::expected<void, Error> addChild(THandle parent, THandle child, ObjectName name = {});
		[[nodiscard]] auto children(THandle handle) const										-> std::expected<Vector<THandle>, Error>;
		[[nodiscard]] auto parent(THandle handle) const											-> std::expected<std::optional<THandle>, Error>;

		THandle root{};									///< Root node handle; invalid means empty tree.
	};

} // namespace vve::simple

namespace vve::simple {

	/// @brief Adds an existing handle as a graph node.
	template <typename THandle> std::expected<void, Error> Graph<THandle>::addNode(THandle handle, ObjectName name) {
		if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
		if (const auto [_, inserted] = nodes_.emplace(handle, std::move(name)); !inserted) {
			return std::unexpected(Error::duplicate_object);
		}
		return {};
	}

	/// @brief Adds one directed edge without forcing graph validation at construction time.
	template <typename THandle> void Graph<THandle>::addEdge(THandle from, THandle to) {
		outgoing_.emplace(from, to);
		incoming_.emplace(to, from);
	}


	/// @brief Returns whether a node handle is registered.
	template <typename THandle> bool Graph<THandle>::contains(THandle handle) const { return nodes_.contains(handle); }

	/// @brief Returns direct outgoing neighbors.
	template <typename THandle> Vector<THandle> Graph<THandle>::children(THandle handle) const {
		Vector<THandle> result{};
		const auto [first, last] = outgoing_.equal_range(handle);
		for (auto it = first; it != last; ++it) { result.push_back(it->second); }
		return result;
	}

	/// @brief Returns direct incoming neighbors.
	template <typename THandle> Vector<THandle> Graph<THandle>::parents(THandle handle) const {
		Vector<THandle> result{};
		const auto [first, last] = incoming_.equal_range(handle);
		for (auto it = first; it != last; ++it) { result.push_back(it->second); }
		return result;
	}

	/// @brief Sets or replaces the root handle.
	template <typename THandle> std::expected<void, Error> Tree<THandle>::setRoot(THandle handle, ObjectName name) {
		if (!handle.valid()) { return std::unexpected(Error::invalid_handle); }
		root = handle;
		if (Base::contains(handle)) { return {}; }
		return Base::addNode(handle, std::move(name));
	}

	/// @brief Adds a child node and connects it to an existing parent.
	template <typename THandle>
	std::expected<void, Error> Tree<THandle>::addChild(THandle parent_node, THandle child, ObjectName name) {
		if (!Base::contains(parent_node)) { return std::unexpected(Error::missing_object); }
		if (Base::contains(child)) {
			const auto current_parent = parent(child);
			if (current_parent && current_parent->has_value()) { return std::unexpected(Error::duplicate_object); }
		} else if (auto added = Base::addNode(child, std::move(name)); !added) {
			return added;
		}
		Base::addEdge(parent_node, child);
		return {};
	}

	/// @brief Returns the direct children of one tree node.
	template <typename THandle> std::expected<Vector<THandle>, Error> Tree<THandle>::children(THandle handle) const {
		if (!Base::contains(handle)) { return std::unexpected(Error::missing_object); }
		return Base::children(handle);
	}

	/// @brief Returns the direct parent of one tree node, if it is not the root.
	template <typename THandle>
	std::expected<std::optional<THandle>, Error> Tree<THandle>::parent(THandle handle) const {
		if (!Base::contains(handle)) { return std::unexpected(Error::missing_object); }
		const auto values = Base::parents(handle);
		return values.empty() ? std::optional<THandle>{} : std::optional<THandle>{values.front()};
	}

} // namespace vve::simple
