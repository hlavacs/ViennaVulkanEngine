#include <filesystem>

import VEEngine.V4;

int main() {
   vve::v4::TaskGraph graph{};
   const auto upload = graph.addTask(vve::v4::ObjectName{.value = "upload"});
   const auto simulate = graph.addTask(vve::v4::ObjectName{.value = "simulate"});
   const auto render = graph.addTask(vve::v4::ObjectName{.value = "render"});
   if (!upload || !simulate || !render || graph.taskCount() != 3) { return 1; }

   graph.addEdge(*upload, *simulate);
   graph.addEdge(*simulate, *render);
   const auto ordered = graph.topologicalOrder();
   if (!ordered || ordered->size() != 3) { return 2; }
   if ((*ordered)[0] != *upload || (*ordered)[1] != *simulate || (*ordered)[2] != *render) { return 3; }

   const auto name = graph.taskName(*simulate);
   if (!name || name->value != "simulate") { return 4; }

   const auto json = graph.toJson("Unit Task Graph");
   if (!json.contains("\"kind\": \"task_graph\"")) { return 5; }
   if (!json.contains("\"name\": \"Unit Task Graph\"")) { return 6; }
   if (!json.contains("\"name\": \"simulate\"")) { return 7; }
   if (!json.contains("\"from_name\": \"upload\"")) { return 8; }
   if (!json.contains("\"to_name\": \"simulate\"")) { return 9; }

   const auto path = std::filesystem::temp_directory_path() / "vve_task_graph_test.json";
   const auto written = graph.writeJson(path, "Unit Task Graph");
   if (!written || !std::filesystem::exists(path)) { return 10; }
   std::filesystem::remove(path);

   if (!graph.remove(*simulate) || graph.contains(*simulate) || graph.taskCount() != 2) { return 11; }
   const auto after_remove = graph.topologicalOrder();
   if (!after_remove || after_remove->size() != 2) { return 12; }

   const auto missing = graph.taskName(*simulate);
   if (missing || missing.error() != vve::v4::Error::missing_object) { return 13; }

   vve::v4::TaskGraph cycle{};
   const auto a = cycle.addTask(vve::v4::ObjectName{.value = "a"});
   const auto b = cycle.addTask(vve::v4::ObjectName{.value = "b"});
   if (!a || !b) { return 14; }
   cycle.addEdge(*a, *b);
   cycle.addEdge(*b, *a);
   const auto cycle_order = cycle.topologicalOrder();
   if (cycle_order || cycle_order.error() != vve::v4::Error::cycle_detected) { return 15; }

   return 0;
}
