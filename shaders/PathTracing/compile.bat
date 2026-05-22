@echo off

slangc vert.slang -entry main -target spirv -o vert.spv -DRASTERIZER

slangc frag.slang -entry main -target spirv -o frag.spv -DRASTERIZER

slangc combinePass.slang -entry main -target spirv -o combinePass.spv

slangc reprojectionPass.slang -entry main -target spirv -o reprojectionPass.spv

slangc LVC_importance_sum.slang -entry main -target spirv -o importanceReduction.spv

slangc LVC_keep_sum.slang -entry main -target spirv -o keepProbReduction.spv

rem --- Raygen shader ---
slangc rtbasic.slang -entry rgenMain -target spirv -o raygen.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc Direct_Raytracer.slang -entry rgenMain -target spirv -o raygen_direct.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc Indirect_Integrator.slang -entry rgenMain -target spirv -o raygen_indirect.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirTemporal.slang -entry rgenMain -target spirv -o raygen_restir_temporal.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirSpatial.slang -entry rgenMain -target spirv -o raygen_restir_spatial.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirTemporalGI.slang -entry rgenMain -target spirv -o raygen_restirGI_temporal.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirSpatialGI.slang -entry rgenMain -target spirv -o raygen_restirGI_spatial.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc Bidirectional_Integrator.slang -entry rgenMain -target spirv -o raygen_bidirectional.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc LVC_generation_Full.slang -entry rgenMain -target spirv -o raygen_light_vertex_generation_full.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc LVC_generation_random_replacment.slang -entry rgenMain -target spirv -o raygen_light_vertex_generation_random_replacment.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc LVC_generation_weighted_replacment.slang -entry rgenMain -target spirv -o raygen_light_vertex_generation_weighted_replacment.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirTemporalLVC.slang -entry rgenMain -target spirv -o raygen_restirLVC_temporal.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirSpatialLVC.slang -entry rgenMain -target spirv -o raygen_restirLVC_spatial.rgen.spv -DRAY_TRACING

rem --- Miss shader ---
slangc rtbasic.slang -entry rmissMain -target spirv -o miss.rmiss.spv -DRAY_TRACING

rem --- Closest hit shader ---
slangc rtbasic.slang -entry rchitMain -target spirv -o closesthit.rchit.spv -DRAY_TRACING

echo Done.
pause