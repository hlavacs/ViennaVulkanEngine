/// @file RenderOutputTimingTests.cpp
/// @brief Opt-in output timestamp spans use completed slots and leave fresh defaults allocation-free.
#include <vulkan/vulkan_core.h>
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

/// @brief Exercise omitted/active output spans, strictly advancing receipts and ordinary in-flight submissions.
int main() {
	using namespace vve;
	for (auto aa : {Antialiasing::none,Antialiasing::smaa}) {
		auto engine=test::hiddenEngine("output-timing",{320,180});
		auto& render=engine.renderSystem(); auto& gpu=render.forward();
		if (!engine.init() || !render.setAntialiasing(aa) || !engine.renderFrame()) return 1;
		auto& target=gpu.targets.front();
		if (gpu.gpuTimingEnabled || target.gpuQueries) return 2;
		if (aa==Antialiasing::none && (target.postImage.image || target.toneMapPipeline.pipeline || target.smaaPool.descriptorPool)) return 3;
		auto receipt=render.renderDiagnostics(target.handle);
		if (!receipt) return 4;
		// Missing public spans fail before touching the implementation's extended metric storage.
		for (const auto* key : {"gpu.tonemap_ms","gpu.smaa_ms","gpu.frame_ms"}) {
			const auto found=std::ranges::find(*receipt,std::string_view{key},[](const auto& entry) { return std::string_view{entry.first}; });
			if (found==receipt->end() || found->second!="unavailable") return 4;
		}
		if (!receipt || target.gpuQueries || target.gpuMs[4] || target.gpuMs[5] || target.gpuMs[6]) return 4;
#ifndef NDEBUG
		if (!gpu.validationActive()) return 5;
#endif
		const auto idle=gpu.deviceWaitIdleCount();
		render.setGpuTimingReadback(true);
		std::uint64_t previous{};
		// Every transition queues ordinary frames; reads occur only when an existing slot fence completes.
		for (auto neutral : {false,true,false,true}) {
			if (!render.setToneMapping({neutral ? ToneMappingOperator::khronos_neutral : ToneMappingOperator::none,2})) return 6;
			for (auto frame : std::views::iota(0U,10U)) {
				if (!engine.renderFrame()) return 7;
				if (frame<3) continue; // Drain older settings without waiting outside the normal frame-slot fence.
				if (!gpu.gpuTimestampBits || !(gpu.gpuTimestampPeriod>0)) {
					std::println("timestamps.supported=false"); return 8;
				}
				const auto serial=target.timingSerial;
				if (serial<=previous) return 9;
				previous=serial;
				if (!target.gpuMs[4] || !target.gpuMs[5] || !target.gpuMs[6]) return 10;
				const auto total=*target.gpuMs[4],tone=*target.gpuMs[5],smaa=*target.gpuMs[6];
				std::println("serial={} gpu.frame_ms={} gpu.tonemap_ms={} gpu.smaa_ms={}",serial,total,tone,smaa);
				if (!std::isfinite(total) || total<=0 || tone<0 || smaa<0 || tone+smaa>total) return 11;
				if ((neutral ? tone<=0 : tone!=0) || (aa==Antialiasing::smaa ? smaa<=0 : smaa!=0)) return 12;
			}
		}
		render.setGpuTimingReadback(false);
		for (auto frame : std::views::iota(0U,4U)) if (!engine.renderFrame()) return 13;
		if (target.gpuMs[4] || target.gpuMs[5] || target.gpuMs[6] || target.timingSerial || gpu.deviceWaitIdleCount()!=idle) return 14;
		render.waitIdle(); engine.gui().shutdownVulkan(); render.shutdown();
		if (gpu.validationErrorCount()) return 15;
	}
	return 0;
}
