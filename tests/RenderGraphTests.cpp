#include <filesystem>

import VEEngine.V4;

int main() {
   vve::v4::RenderGraph graph{};
   const auto shadow = graph.addNode(vve::v4::ObjectName{.value = "shadow"});
   const auto forward = graph.addNode(vve::v4::ObjectName{.value = "forward"});
   const auto present = graph.addNode(vve::v4::ObjectName{.value = "present"});
   if (!shadow || !forward || !present || graph.nodeCount() != 3) { return 1; }

   graph.addEdge(*shadow, *forward);
   graph.addEdge(*forward, *present);
   const auto ordered = graph.topologicalOrder();
   if (!ordered || ordered->size() != 3) { return 2; }
   if ((*ordered)[0] != *shadow || (*ordered)[1] != *forward || (*ordered)[2] != *present) { return 3; }

   const auto pass_name = graph.nodeName(*forward);
   if (!pass_name || pass_name->value != "forward") { return 4; }

   const auto forward_handle = graph.nodeHandle("forward");
   if (!forward_handle || *forward_handle != *forward) { return 5; }

   const auto json = graph.toJson("render_graph", "Unit Render Graph");
   if (!json.contains("\"kind\": \"render_graph\"")) { return 6; }
   if (!json.contains("\"name\": \"Unit Render Graph\"")) { return 7; }
   if (!json.contains("\"name\": \"forward\"")) { return 8; }
   if (!json.contains("\"from_name\": \"shadow\"")) { return 9; }
   if (!json.contains("\"to_name\": \"forward\"")) { return 10; }

   const auto path = std::filesystem::temp_directory_path() / "vve_render_graph_test.json";
   const auto written = graph.writeJson(path, "render_graph", "Unit Render Graph");
   if (!written || !std::filesystem::exists(path)) { return 11; }
   std::filesystem::remove(path);

   if (!graph.remove(*forward) || graph.contains(*forward) || graph.nodeCount() != 2) { return 12; }
   const auto after_remove = graph.topologicalOrder();
   if (!after_remove || after_remove->size() != 2) { return 13; }

   vve::v4::RenderGraph cycle{};
   const auto a = cycle.addNode(vve::v4::ObjectName{.value = "a"});
   const auto b = cycle.addNode(vve::v4::ObjectName{.value = "b"});
   if (!a || !b) { return 14; }
   cycle.addEdge(*a, *b);
   cycle.addEdge(*b, *a);
   const auto cycle_order = cycle.topologicalOrder();
   if (cycle_order || cycle_order.error() != vve::v4::Error::cycle_detected) { return 15; }

   return 0;
}
