@echo off

slangc vert.slang -entry main -target spirv -o vert.spv -DRASTERIZER 

slangc frag.slang -entry main -target spirv -o frag.spv -DRASTERIZER

slangc combinePass.slang -entry main -target spirv -o combinePass.spv

slangc reprojectionPass.slang -entry main -target spirv -o reprojectionPass.spv -fvk-use-scalar-layout


slangc IR_Morton_Code.slang -entry main -target spirv -o IR_Morton_Code.spv -fvk-use-scalar-layout

slangc IR_Radix.slang -entry main -target spirv -o IR_Radix.spv -fvk-use-scalar-layout

slangc IR_Radix_Histogram.slang -entry main -target spirv -o IR_Radix_Histogram.spv -fvk-use-scalar-layout

slangc IR_PDF_Estimation.slang -entry main -target spirv -o IR_PDF_Estimation.spv -fvk-use-scalar-layout

slangc IR_PDF_Estimation_Range_Search.slang -entry main -target spirv -o IR_PDF_Estimation_Range_Search.spv -fvk-use-scalar-layout

slangc IR_Scoring.slang -entry main -target spirv -o IR_Scoring_PixelUsage.spv -fvk-use-scalar-layout

slangc IR_Scoring_Intensity.slang -entry main -target spirv -o IR_Scoring_Intensity.spv -fvk-use-scalar-layout

slangc IR_Set_Replacment_Limits.slang -entry main -target spirv -o IR_Set_Replacment_Limits.spv -fvk-use-scalar-layout


rem --- Raygen shader ---
slangc rtbasic.slang -entry rgenMain -target spirv -o raygen.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc Direct_Raytracer.slang -entry rgenMain -target spirv -o raygen_direct.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc Indirect_Integrator.slang -entry rgenMain -target spirv -o raygen_indirect.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Raygen shader ---
slangc RestirTemporalGI.slang -entry rgenMain -target spirv -o raygen_restirGI_temporal.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Raygen shader ---
slangc RestirSpatialGI.slang -entry rgenMain -target spirv -o raygen_restirGI_spatial.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Raygen shader ---
slangc IR_VPL_Generation_Random_Replacment.slang -entry rgenMain -target spirv -o raygen_vpl_generation_random_replacment.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Raygen shader ---
slangc IR_VPL_Generation_Evolutionary.slang -entry rgenMain -target spirv -o IR_VPL_Generation_Evolutionary.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Raygen shader ---
slangc IR_Restir_Temporal.slang -entry rgenMain -target spirv -o raygen_restir_IR_temporal.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Raygen shader ---
slangc IR_Restir_Spatial.slang -entry rgenMain -target spirv -o raygen_restir_IR_spatial.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Raygen shader ---
slangc IR_VPL_test.slang -entry rgenMain -target spirv -o raygen_IR_VPL_Test.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Raygen shader ---
slangc IR_naive_sampling.slang -entry rgenMain -target spirv -o raygen_IR_naive_sampling.rgen.spv -DRAY_TRACING -fvk-use-scalar-layout

rem --- Miss shader ---
slangc rtbasic.slang -entry rmissMain -target spirv -o miss.rmiss.spv -DRAY_TRACING

rem --- Closest hit shader ---
slangc rtbasic.slang -entry rchitMain -target spirv -o closesthit.rchit.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirTemporal.slang -entry rgenMain -target spirv -o raygen_restir_temporal.rgen.spv -DRAY_TRACING

rem --- Raygen shader ---
slangc RestirSpatial.slang -entry rgenMain -target spirv -o raygen_restir_spatial.rgen.spv -DRAY_TRACING

echo Done.
pause