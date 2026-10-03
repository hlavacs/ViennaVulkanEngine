/// @file
/// @brief Verifies graph membership and creation order without relying on sorted handle values.
import std;
import VVEngine.Simple;

/// @brief Checks duplicate/invalid insertions and root-first tree traversal with nonmonotonic handles.
int main() {
   using vve::NodeHandle;
   const auto root = vve::makeHandleForTest<NodeHandle>(30U);
   const auto child = vve::makeHandleForTest<NodeHandle>(10U);
   const auto grandchild = vve::makeHandleForTest<NodeHandle>(20U);
   const auto sibling = vve::makeHandleForTest<NodeHandle>(5U);
   const vve::Vector<NodeHandle> expected{root, child, grandchild, sibling};
   vve::simple::Graph<NodeHandle> graph{};
   if (!graph.nodes().empty() || graph.contains(root)) { return 1; }
   const auto invalid = graph.addNode(NodeHandle{});
   if (invalid || invalid.error() != vve::Error::invalid_handle || !graph.nodes().empty()) { return 2; }
   // Creation order follows insertion even when numeric handles run backward.
   for (const auto handle : expected) {
      if (!graph.addNode(handle) || !graph.contains(handle)) { return 3; }
   }
   const auto duplicate = graph.addNode(child);
   if (duplicate || duplicate.error() != vve::Error::duplicate_object ||
       !std::ranges::equal(graph.nodes(), expected)) { return 4; }

   vve::simple::Tree<NodeHandle> tree{};
   if (!tree.setRoot(root) || !tree.addChild(root, child) || !tree.addChild(child, grandchild) ||
       !tree.addChild(root, sibling) || !tree.setRoot(root)) { return 5; }
   if (tree.root != root || !std::ranges::equal(tree.nodes(), expected)) { return 6; }
   const auto duplicate_child = tree.addChild(root, child);
   const auto missing_parent = tree.addChild(NodeHandle{}, sibling);
   if (duplicate_child || duplicate_child.error() != vve::Error::duplicate_object ||
       missing_parent || missing_parent.error() != vve::Error::missing_object ||
       !std::ranges::equal(tree.nodes(), expected)) { return 7; }
   const auto parent = tree.parent(grandchild);
   if (!parent || *parent != child) { return 8; }
   std::println("D7 graph_nodes={} tree_nodes={} creation_order=1 rejected_insertions_preserve_order=1",
      graph.nodes().size(), tree.nodes().size());
   return 0;
}
