import VEEngine.V4;

int main() {
   vve::v4::RenderGraph graph{};
   const auto shadow = graph.addPass(vve::v4::ObjectName{.value = "shadow"});
   const auto forward = graph.addPass(vve::v4::ObjectName{.value = "forward"});
   const auto present = graph.addPass(vve::v4::ObjectName{.value = "present"});
   if (!shadow || !forward || !present || graph.passCount() != 3) { return 1; }

   graph.addEdge(*shadow, *forward);
   graph.addEdge(*forward, *present);
   const auto ordered = graph.topologicalOrder();
   if (!ordered || ordered->size() != 3) { return 2; }
   if ((*ordered)[0] != *shadow || (*ordered)[1] != *forward || (*ordered)[2] != *present) { return 3; }

   const auto pass_name = graph.passName(*forward);
   if (!pass_name || pass_name->value != "forward") { return 4; }

   if (!graph.remove(*forward) || graph.contains(*forward) || graph.passCount() != 2) { return 5; }
   const auto after_remove = graph.topologicalOrder();
   if (!after_remove || after_remove->size() != 2) { return 6; }

   vve::v4::RenderGraph cycle{};
   const auto a = cycle.addPass(vve::v4::ObjectName{.value = "a"});
   const auto b = cycle.addPass(vve::v4::ObjectName{.value = "b"});
   if (!a || !b) { return 7; }
   cycle.addEdge(*a, *b);
   cycle.addEdge(*b, *a);
   const auto cycle_order = cycle.topologicalOrder();
   if (cycle_order || cycle_order.error() != vve::v4::Error::cycle_detected) { return 8; }

   return 0;
}
