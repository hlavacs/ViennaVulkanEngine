export module VEEngine.Simple:Graph;
import std;
export import :Types;

/// @file
/// @brief Generic named DAG and tree helpers shared by simple task, render, and scene structures.

export namespace vve::simple {

	/// @brief Small named DAG with reverse edges for parent lookup and JSON inspection.
	template <typename THandle> class Graph {
	public:
		[[nodiscard]] std::expected<THandle, Error> addNode(ObjectName name = {});
		[[nodiscard]] std::expected<void, Error> addNode(THandle handle, ObjectName name = {});
		auto addEdge(THandle from, THandle to)														-> void;
		[[nodiscard]] auto contains(THandle handle) const										-> bool;
		[[nodiscard]] auto nodeName(THandle handle) const										-> std::expected<ObjectName, Error>;
		[[nodiscard]] auto children(THandle handle) const										-> Vector<THandle>;
		[[nodiscard]] auto parents(THandle handle) const										-> Vector<THandle>;
		[[nodiscard]] auto topologicalOrder() const												-> std::expected<Vector<THandle>, Error>;
		[[nodiscard]] auto nodeCount() const														-> std::size_t;
		[[nodiscard]] auto toJson(std::string_view kind, std::string_view name) const	-> std::string;
		[[nodiscard]] std::expected<void, Error> writeJson(const std::filesystem::path &path,
																			std::string_view kind, std::string_view name) const;

	private:
		using EdgeMap = std::unordered_multimap<THandle, THandle, HandleHash<THandle>>;


		std::map<THandle, ObjectName> nodes_{};	///< Node labels by handle.
		EdgeMap outgoing_{};								///< Forward dependency edges.
		EdgeMap incoming_{};								///< Reverse dependency edges.
	};

	/// @brief Tree view over the generic graph, keeping only one root handle as tree-specific state.
	template <typename THandle> class Tree : public Graph<THandle> {
	private:
		using Base = Graph<THandle>;					///< Reused graph storage and traversal implementation.

	public:
		[[nodiscard]] std::expected<void, Error> setRoot(THandle handle, ObjectName name = {});
		[[nodiscard]] std::expected<void, Error> addChild(THandle parent, THandle child, ObjectName name = {});
		[[nodiscard]] auto children(THandle handle) const										-> std::expected<Vector<THandle>, Error>;
		[[nodiscard]] auto parent(THandle handle) const											-> std::expected<std::optional<THandle>, Error>;

		THandle root{};									///< Root node handle; invalid means empty tree.
	};

} // namespace vve::simple

namespace vve::simple::detail {

	auto appendJsonString(std::string &output, std::string_view text)										-> void;
	template <typename THandle> [[nodiscard]] std::string jsonHandleId(THandle handle);
	[[nodiscard]] auto writeJsonFile(const std::filesystem::path &path, std::string_view json)	-> std::expected<void, Error>;

} // namespace vve::simple::detail

namespace vve::simple {

	/// @brief Adds a generated counter node and stores its optional display name.
	template <typename THandle> std::expected<THandle, Error> Graph<THandle>::addNode(ObjectName name) {
		const auto handle = makeCounterHandle<THandle>();
		if (auto added = addNode(handle, std::move(name)); !added) { return std::unexpected(added.error()); }
		return handle;
	}

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

	/// @brief Returns the stored node name.
	template <typename THandle> std::expected<ObjectName, Error> Graph<THandle>::nodeName(THandle handle) const {
		const auto node = nodes_.find(handle);
		if (node == nodes_.end()) { return std::unexpected(Error::missing_object); }
		return node->second;
	}

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

	/// @brief Returns nodes in dependency order and rejects invalid edges or cycles.
	template <typename THandle> std::expected<Vector<THandle>, Error> Graph<THandle>::topologicalOrder() const {
		std::map<THandle, std::uint32_t> incoming_counts{};
		std::map<THandle, Vector<THandle>> ordered_children{};
		for (const auto &[handle, _] : nodes_) { incoming_counts.try_emplace(handle, 0); }

		for (const auto &[from, to] : outgoing_) {
			if (!from.valid() || !to.valid()) { return std::unexpected(Error::invalid_handle); }
			if (!incoming_counts.contains(from) || !incoming_counts.contains(to)) {
				return std::unexpected(Error::missing_object);
			}
			ordered_children[from].push_back(to);
			++incoming_counts[to];
		}

		std::set<THandle> ready{};
		Vector<THandle> ordered{};
		ordered.reserve(incoming_counts.size());
		for (const auto &[node, count] : incoming_counts) {
			if (count == 0) { ready.insert(node); }
		}

		while (!ready.empty()) {
			const auto node = *ready.begin();
			ready.erase(ready.begin());
			ordered.push_back(node);
			for (const auto child : ordered_children[node]) {
				auto &count = incoming_counts[child];
				if (--count == 0) { ready.insert(child); }
			}
		}

		if (ordered.size() != incoming_counts.size()) { return std::unexpected(Error::cycle_detected); }
		return ordered;
	}

	/// @brief Returns the number of registered nodes.
	template <typename THandle> std::size_t Graph<THandle>::nodeCount() const { return nodes_.size(); }



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

namespace vve::simple::detail {

	/// @brief Appends one JSON string literal with minimal escaping.
	inline auto appendJsonString(std::string &output, std::string_view text)							-> void{
		output.push_back('"');
		for (const char ch : text) {
			switch (ch) {
			case '"': output += "\\\""; break;
			case '\\': output += "\\\\"; break;
			case '\b': output += "\\b"; break;
			case '\f': output += "\\f"; break;
			case '\n': output += "\\n"; break;
			case '\r': output += "\\r"; break;
			case '\t': output += "\\t"; break;
			default: output.push_back(static_cast<unsigned char>(ch) < 0x20U ? ' ' : ch); break;
			}
		}
		output.push_back('"');
	}

	/// @brief Returns a stable JSON id for a typed handle.
	template <typename THandle> inline std::string jsonHandleId(THandle handle) {
		return std::to_string(handle.value);
	}

	/// @brief Writes a UTF-8 JSON string to disk, creating the parent directory when needed.
	inline auto writeJsonFile(const std::filesystem::path &path, std::string_view json)				-> std::expected<void, Error>{
		std::error_code error{};
		if (!path.parent_path().empty()) { std::filesystem::create_directories(path.parent_path(), error); }
		if (error) { return std::unexpected(Error::internal_error); }

		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		if (!output.is_open()) { return std::unexpected(Error::internal_error); }
		output << json;
		return {};
	}

} // namespace vve::simple::detail

namespace vve::simple {

	/// @brief Returns a simple node-and-edge JSON dump for graph visualization tools.
	template <typename THandle>
	std::string Graph<THandle>::toJson(std::string_view kind, std::string_view name) const {
		std::string json{};
		json += "{\n  \"kind\": ";
		detail::appendJsonString(json, kind);
		json += ",\n  \"name\": ";
		detail::appendJsonString(json, name);
		json += ",\n  \"nodes\": [";

		bool first_node{true};
		for (const auto &[handle, node_name] : nodes_) {
			json += first_node ? "\n" : ",\n";
			first_node = false;
			json += "    {\"id\": ";
			detail::appendJsonString(json, detail::jsonHandleId(handle));
			json += ", \"name\": ";
			detail::appendJsonString(json, node_name.value);
			json += "}";
		}

		json += "\n  ],\n  \"edges\": [";
		std::vector<std::pair<THandle, THandle>> edges{};
		edges.reserve(outgoing_.size());
		for (const auto &[from, to] : outgoing_) { edges.emplace_back(from, to); }
		std::ranges::sort(edges, [](const auto &lhs, const auto &rhs) {
			if (lhs.first == rhs.first) { return lhs.second < rhs.second; }
			return lhs.first < rhs.first;
		});

		bool first_edge{true};
		for (const auto [from, to] : edges) {
			json += first_edge ? "\n" : ",\n";
			first_edge = false;
			json += "    {\"from\": ";
			detail::appendJsonString(json, detail::jsonHandleId(from));
			json += ", \"to\": ";
			detail::appendJsonString(json, detail::jsonHandleId(to));
			json += ", \"from_name\": ";
			detail::appendJsonString(json, nodes_.contains(from) ? nodes_.at(from).value : "");
			json += ", \"to_name\": ";
			detail::appendJsonString(json, nodes_.contains(to) ? nodes_.at(to).value : "");
			json += "}";
		}

		json += "\n  ]\n}\n";
		return json;
	}

	/// @brief Writes the graph JSON dump to disk.
	template <typename THandle>
	std::expected<void, Error> Graph<THandle>::writeJson(const std::filesystem::path &path,
																			std::string_view kind, std::string_view name) const {
		return detail::writeJsonFile(path, toJson(kind, name));
	}

} // namespace vve::simple
