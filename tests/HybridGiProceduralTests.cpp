/** @file @brief Native procedural pass contracts and deterministic linear-HDR transport receipts. */
#include <vulkan/vulkan.h>
#include "vertexMain.h"
#include "gbufferMain.h"
#include "captureMain.h"
#include "plainMain.h"
#include "forwardMain.h"
import std;
import VVEngine;
import VVEngine.Simple;
import VVEngine.Simple.Renderer;
import VVE.TestSupport;

namespace {
using namespace vve;
const std::filesystem::path reports{VVE_NATIVE_TEST_REPORT_DIR}; ///< Configurable transient capture evidence, outside sources for AI-loop runs.
/// @brief Debug requires its messenger; Release requires CTest's externally loaded synchronization layer and diagnostic failure gate.
bool requiredValidation(const vve::simple::ForwardRenderer& renderer) {
#ifndef NDEBUG
    return renderer.validationActive();
#else
    const auto* layers=std::getenv("VK_INSTANCE_LAYERS");
    const auto* sync=std::getenv("VK_LAYER_VALIDATE_SYNC");
    // Successful Vulkan instance creation with this explicit layer request also rejects an unavailable layer.
    const bool external=layers && std::string_view{layers}.find("VK_LAYER_KHRONOS_validation")!=std::string_view::npos
        && sync && std::string_view{sync}=="1";
    std::println("validation.external_sync={} validation.checked_by=CTest-failure-expression",external);
    return external;
#endif
}
unsigned destroyed{}; ///< Measured application owner destruction, including fence-delayed callback retirement.
/// @brief Test-only 128-byte shader constants; no private engine uniform/material ABI is used.
struct Push {
    Mat4 clip{}; ///< Pass clip transform.
    Vec4 low{},high{},color{},eye{}; ///< Box corners, linear color/opacity and capture/probe parameters.
};
static_assert(sizeof(Push)==128);
/// @brief Application-owned Vulkan pipelines and CPU boxes; the callback alone owns their GPU lifetime.
struct Geometry {
    VkDevice device{}; ///< Borrowed until the callback's fence retirement.
    VkDescriptorSetLayout setLayout{}; VkPipelineLayout layout{}; VkDescriptorPool pool{}; ///< Application descriptors, independent of engine layouts.
    std::map<unsigned,VkPipeline> pipelines; ///< Pass/coverage-specific pipeline cache.
    std::map<std::pair<std::uint64_t,unsigned>,VkDescriptorSet> sets; ///< Immutable probe bindings by window/fenced slot.
    Vector<CuboidDescriptor> boxes; ///< Whole procedural scene, independent of main camera visibility.
    std::array<unsigned,5> calls{}; unsigned preparations{},sunCalls{},mainSamples{}; ///< Measured pass invocations and prepared coverage.
    bool transparent{},valid{true},differentCapture{},differentShadow{}; ///< Surface mode and observed per-pass view checks.
    float opacity{.5F}; ///< Transparent diagnostic factor, compared with a full-opacity reference.
    Mat4 mainView{}; ///< Main view from the outside-rendering preparation.
    Geometry()=default;
    ~Geometry();
    void initialize(const NativeDrawContext &);
    VkPipeline pipeline(const NativeDrawContext &);
    std::expected<void,Error> record(const NativeDrawContext &);
};
/// @brief Turn Vulkan failure into a callback error, including partially created pipeline owners.
void require(VkResult result) { if (result!=VK_SUCCESS) throw Error::platform_error; }
/// @brief Destruction happens on VVE's completed-submit retirement boundary, before device destruction.
Geometry::~Geometry() {
    ++destroyed;
    if (!device) return; // A recorder may be cleared before the first drawable frame.
    for (const auto &[key,pipeline] : pipelines) vkDestroyPipeline(device,pipeline,nullptr);
    vkDestroyDescriptorPool(device,pool,nullptr); vkDestroyPipelineLayout(device,layout,nullptr); vkDestroyDescriptorSetLayout(device,setLayout,nullptr);
}
/// @brief Allocate two public sampled-image bindings; no engine descriptor layouts are imported.
void Geometry::initialize(const NativeDrawContext &c) {
    device=reinterpret_cast<VkDevice>(c.device);
    const std::array<VkDescriptorSetLayoutBinding,2> bindings{{{0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}}};
    const VkDescriptorSetLayoutCreateInfo info{.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,.bindingCount=2,.pBindings=bindings.data()};
    require(vkCreateDescriptorSetLayout(device,&info,nullptr,&setLayout));
    const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(Push)};
    const VkPipelineLayoutCreateInfo layoutInfo{.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,.setLayoutCount=1,.pSetLayouts=&setLayout,.pushConstantRangeCount=1,.pPushConstantRanges=&push};
    require(vkCreatePipelineLayout(device,&layoutInfo,nullptr,&layout));
    const VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,32};
    const VkDescriptorPoolCreateInfo poolInfo{.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,.maxSets=16,.poolSizeCount=1,.pPoolSizes=&size};
    require(vkCreateDescriptorPool(device,&poolInfo,nullptr,&pool));
    VkPhysicalDeviceProperties physical{}; vkGetPhysicalDeviceProperties(reinterpret_cast<VkPhysicalDevice>(c.physicalDevice),&physical);
    std::println("native.device={} native.samples={}",std::string_view{physical.deviceName},c.samples);
}
/// @brief Build a dynamic-rendering pipeline directly from the public formats/sample count.
VkPipeline Geometry::pipeline(const NativeDrawContext &c) {
    const bool shared=c.probeCascades!=0;
    const auto key=unsigned(c.pass)*16+c.samples+(shared ? 128 : 0);
    if (pipelines.contains(key)) return pipelines.at(key);
    const bool shadow=c.pass==NativeDrawPass::shadow,blend=c.pass==NativeDrawPass::forward_transparent;
    const std::span<const std::uint32_t> code=c.pass==NativeDrawPass::gbuffer ? std::span<const std::uint32_t>{native_gbufferMain} : c.pass==NativeDrawPass::probe_capture ? std::span<const std::uint32_t>{native_captureMain}
        : shared ? std::span<const std::uint32_t>{native_forwardMain} : std::span<const std::uint32_t>{native_plainMain};
    const char *entry=c.pass==NativeDrawPass::gbuffer ? "gbufferMain" : c.pass==NativeDrawPass::probe_capture ? "captureMain" : shared ? "forwardMain" : "plainMain";
    std::array<VkShaderModule,2> modules{};
    const auto module=[&](std::span<const std::uint32_t> bytes,VkShaderModule &m) {
        const VkShaderModuleCreateInfo info{.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,.codeSize=bytes.size_bytes(),.pCode=bytes.data()};
        require(vkCreateShaderModule(device,&info,nullptr,&m));
    };
    VkPipeline created{};
    try {
        module(native_vertexMain,modules[0]); if (!shadow) module(code,modules[1]);
        const std::array<VkPipelineShaderStageCreateInfo,2> stages{{{.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_VERTEX_BIT,.module=modules[0],.pName="vertexMain"},
            {.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,.stage=VK_SHADER_STAGE_FRAGMENT_BIT,.module=modules[1],.pName=entry}}};
        const VkPipelineVertexInputStateCreateInfo vertices{.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        const VkPipelineInputAssemblyStateCreateInfo assembly{.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
        const VkPipelineViewportStateCreateInfo viewport{.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,.viewportCount=1,.scissorCount=1};
        const VkPipelineRasterizationStateCreateInfo raster{.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,.polygonMode=VK_POLYGON_MODE_FILL,.cullMode=VK_CULL_MODE_NONE,.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,.lineWidth=1};
        const VkPipelineMultisampleStateCreateInfo samples{.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,.rasterizationSamples=VkSampleCountFlagBits(c.samples)};
        const VkPipelineDepthStencilStateCreateInfo depth{.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,.depthTestEnable=VK_TRUE,.depthWriteEnable=!blend,.depthCompareOp=VK_COMPARE_OP_LESS};
        std::array<VkPipelineColorBlendAttachmentState,3> colors{};
        for (auto &a:colors) a={.blendEnable=blend,.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA,.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,.colorBlendOp=VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,.alphaBlendOp=VK_BLEND_OP_ADD,.colorWriteMask=15};
        const VkPipelineColorBlendStateCreateInfo blending{.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,.attachmentCount=c.colorAttachmentCount,.pAttachments=colors.data()};
        const std::array states{VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};
        const VkPipelineDynamicStateCreateInfo dynamic{.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,.dynamicStateCount=2,.pDynamicStates=states.data()};
        std::array<VkFormat,3> formats{}; std::ranges::transform(c.colorFormats,formats.begin(),[](auto f) { return VkFormat(f); });
        const VkPipelineRenderingCreateInfo rendering{.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,.colorAttachmentCount=c.colorAttachmentCount,.pColorAttachmentFormats=formats.data(),.depthAttachmentFormat=VkFormat(c.depthFormat)};
        const VkGraphicsPipelineCreateInfo info{.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,.pNext=&rendering,.stageCount=shadow ? 1U : 2U,.pStages=stages.data(),.pVertexInputState=&vertices,
            .pInputAssemblyState=&assembly,.pViewportState=&viewport,.pRasterizationState=&raster,.pMultisampleState=&samples,.pDepthStencilState=&depth,.pColorBlendState=&blending,.pDynamicState=&dynamic,.layout=layout};
        require(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&created));
    } catch (...) { for (auto m:modules) vkDestroyShaderModule(device,m,nullptr); throw; }
    for (auto m:modules) vkDestroyShaderModule(device,m,nullptr);
    pipelines.emplace(key,created); return created;
}
/// @brief Record all scene boxes with the actual pass view; per-draw push constants cannot overwrite another face's data.
std::expected<void,Error> Geometry::record(const NativeDrawContext &c) try {
    if (!device) initialize(c);
    if (c.prepare) { ++preparations; mainView=c.view; mainSamples=c.samples; return {}; }
    ++calls[unsigned(c.pass)]; sunCalls+=c.probeShadow;
    const float difference=test::rgbDelta(Vec3{c.view[3]},Vec3{mainView[3]});
    differentCapture |= c.pass==NativeDrawPass::probe_capture && difference>.01F;
    differentShadow |= c.pass==NativeDrawPass::shadow && difference>.01F;
    valid &= c.samples==(c.pass==NativeDrawPass::shadow || c.pass==NativeDrawPass::probe_capture ? 1U : mainSamples) &&
        c.depthFormat==VK_FORMAT_D32_SFLOAT && test::rgbDelta(c.eye.value,Vec3{math::inverse(c.view)[3]})<.0001F;
    const unsigned attachments=c.pass==NativeDrawPass::shadow ? 0 : c.pass==NativeDrawPass::gbuffer ? 3 : 1;
    valid &= c.colorAttachmentCount==attachments;
    for (auto i:std::views::iota(0U,attachments)) valid &= c.colorFormats[i]==VK_FORMAT_R16G16B16A16_SFLOAT;
    if (c.pass==NativeDrawPass::probe_capture) valid &= c.faceIndex<6 && c.cascadeIndex<2 && c.width==16 && c.height==16 && c.viewportX==c.faceIndex*16;
    const auto expected=math::multiply(c.projection,c.view);
    valid &= std::memcmp(&expected,&c.viewProjection,sizeof(expected))==0;
    // Check the near-plane centre lies inside every supplied culling plane.
    auto centre=math::multiply(math::inverse(c.viewProjection),Vec4{0,0,.5F,1}); centre=math::scale(centre,1/centre.w);
    for (auto plane:c.frustum) valid &= math::dot(plane,centre)>-.001F;
    const bool forward=c.pass==NativeDrawPass::forward_transparent;
    if ((transparent && !forward) || (!transparent && forward) || c.pass==NativeDrawPass::forward_opaque && c.colorAttachmentCount!=1) return {};
    const auto cmd=reinterpret_cast<VkCommandBuffer>(c.commandBuffer);
    const VkViewport viewport{float(c.viewportX),float(c.viewportY),float(c.width),float(c.height),0,1};
    const VkRect2D scissor{{int(c.viewportX),int(c.viewportY)},{c.width,c.height}};
    vkCmdSetViewport(cmd,0,1,&viewport); vkCmdSetScissor(cmd,0,1,&scissor);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline(c));
    if (forward && c.probeCascades) {
        auto &set=sets[{c.window.value,c.frameIndex}];
        if (!set) {
            const VkDescriptorSetAllocateInfo allocation{.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,.descriptorPool=pool,.descriptorSetCount=1,.pSetLayouts=&setLayout};
            require(vkAllocateDescriptorSets(device,&allocation,&set));
            // Probe views persist across resize; each window/slot descriptor is written only once after its fence.
            for (auto i:std::views::iota(0U,2U)) {
                const VkDescriptorImageInfo image{.imageView=reinterpret_cast<VkImageView>(c.probeIrradianceViews[i]),.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                const VkWriteDescriptorSet write{.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,.dstSet=set,.dstBinding=i,.descriptorCount=1,.descriptorType=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,.pImageInfo=&image};
                vkUpdateDescriptorSets(device,1,&write,0,nullptr);
            }
        }
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,layout,0,1,&set,0,nullptr);
    }
    for (const auto &b:boxes) {
        Push push{.clip=c.viewProjection,.low=Vec4{b.minimum,c.probeControl.y},.high=Vec4{b.maximum,c.probeControl.x},.color=Vec4{b.color.value,transparent ? opacity : 1.0F},
            .eye=c.pass==NativeDrawPass::probe_capture ? Vec4{c.eye.value,c.captureFar} : Vec4{c.probeEye.value,Scalar(c.probeCascades)}};
        vkCmdPushConstants(cmd,layout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),&push);
        vkCmdDraw(cmd,transparent ? 6 : 36,1,transparent ? 12 : 0,0); // A transparent surface is a single top face, not two overlapping sheets.
    }
    // Deliberately leave altered dynamic state; VVE must restore it before later engine geometry or fullscreen lighting.
    const VkViewport tiny{0,0,1,1,0,1}; const VkRect2D tinyScissor{{0,0},{1,1}};
    vkCmdSetViewport(cmd,0,1,&tiny); vkCmdSetScissor(cmd,0,1,&tinyScissor);
    return {};
} catch (Error e) { return std::unexpected(e); }
catch (...) { return std::unexpected(Error::platform_error); }
/// @brief Registration uses the same public signature in facade smoke tests and numerical fixtures.
template<class TRender> Geometry *install(TRender &r,bool transparent=false) {
    auto owner=std::make_shared<Geometry>(); owner->transparent=transparent; auto *geometry=owner.get();
    if (!r.setNativeDraw([owner](const NativeDrawContext &c) { return owner->record(c); })) return nullptr;
    return geometry;
}
constexpr Vec3 red{1,.01F,.01F},green{.01F,1,.01F}; ///< Only chromatic sources in the transport fixtures.
/// @brief Describe a test box with facade vocabulary only; procedural world coordinates are shader-generated.
CuboidDescriptor box(Vec3 low,Vec3 high,Vec3 color) { return {.minimum=low,.maximum=high,.color=LinearColor{color}}; }
/// @brief Exercise facade registration with actual draws for each backend and both coverage counts, including replacement/clear.
bool passes() {
    bool pass=true;
    for (auto backend:{RendererBackend::forward,RendererBackend::deferred,RendererBackend::hybrid_gi}) {
        for (auto aa:{Antialiasing::none,Antialiasing::msaa4}) {
            const auto oldDestroyed=destroyed;
            Engine<> engine{WindowSetups{WindowSetup{}.id("main").extent({64,64}).visible(false)}};
            auto world=engine.world(); auto &r=world.get<RenderSystem>();
            if (!engine.init() || !r.setRendererBackend(backend) || !r.setAntialiasing(aa)) return false;
            r.setCamera(Camera::lookAt(Position{Vec3{0,1.5F,3}},Position{}));
            r.setDirectionalLight(Direction{Vec3{-1,-.5F,0}},LinearColor{},LightIntensity{4},LinearColor{Vec3{0}});
            auto *g=install(r); if (!g) return false;
            g->boxes.push_back(box(Vec3{-.4F,0,-.4F},Vec3{.4F,1,.4F},red));
            if (!r.addPlane(Vec2{2,2},LinearColor{}) || !engine.step()) return false;
            const bool deferred=backend!=RendererBackend::forward,gi=backend==RendererBackend::hybrid_gi;
            pass &= g->valid && g->preparations==1 && g->differentShadow && g->calls[unsigned(NativeDrawPass::shadow)]>0 &&
                g->calls[unsigned(NativeDrawPass::forward_transparent)]==1 && g->calls[unsigned(NativeDrawPass::gbuffer)]==unsigned(deferred) &&
                g->calls[unsigned(NativeDrawPass::forward_opaque)]==unsigned(!deferred) && (gi ? g->differentCapture && g->calls[unsigned(NativeDrawPass::probe_capture)]==12 && g->sunCalls==1 : !g->differentCapture);
            std::println("passes.backend={} samples={} prepare={} forward={} gbuffer={} shadow={} capture={} transparent={} capture.distinct={} shadow.distinct={} valid={}",
                unsigned(backend),aa==Antialiasing::none ? 1 : 4,g->preparations,g->calls[0],g->calls[1],g->calls[2],g->calls[3],g->calls[4],g->differentCapture,g->differentShadow,g->valid);
            // Retire the old owner while frames remain in flight; subsequent engine geometry must retain all its bindings.
            if (!r.setNativeDraw({}) || !engine.step() || !engine.step()) return false;
            pass &= destroyed==oldDestroyed+1;
            std::println("native.retired_owners={} native.retired_expected=1",destroyed-oldDestroyed);
        }
    }
    std::println("native.facade.pass={} validation.checked_by=CTest-failure-expression",pass);
    return pass;
}
/// @brief Use paused lighting shared with the established color-bounce fixture; its upward receiver has no direct cosine.
void lighting(simple::Engine &engine) {
    auto &r=engine.renderSystem();
    r.setCamera(Camera::lookAt(Position{Vec3{0,1.5F,3}},Position{}, {},FovY{.45F}));
    r.setBackgroundColor(LinearColor{Vec3{0}});
    r.setDirectionalLight(Direction{Vec3{-1,0,0}},LinearColor{},LightIntensity{4},LinearColor{Vec3{0}});
    r.setPointLight(Position{},LinearColor{},LightIntensity{0},LightRange{1},LinearColor{Vec3{0}});
}
/// @brief Diagnostic checkpoints reuse the existing internal HDR readback hook; ordinary frame drawing remains public-context-only.
std::optional<Vec3> measure(simple::Engine &engine,Vec3 point,std::string_view name) { return test::linearPoint(engine.renderSystem().forward(),point,name); }
/// @brief Off-camera geometry must transport red/green light, then converge back after removal.
bool bounce(simple::Engine &engine) {
    auto &r=engine.renderSystem(); auto *g=install(r);
    if (!g || !r.addPlane(Vec2{1,1},LinearColor{Vec3{.5F}})) return false;
    g->boxes.push_back(box(Vec3{-2.2F,0,-2},Vec3{-2,3,2},red));
    // Every wall corner is strictly outside the main left plane; capture must use another view to see it.
    const auto camera=Camera::lookAt(Position{Vec3{0,1.5F,3}},Position{}, {},FovY{.45F});
    const auto clip=math::multiply(math::perspectiveVulkan(.45F,1,.1F,100),math::lookAt(camera.position.value,Vec3{0},Vec3{0,1,0}));
    for (auto c:std::views::iota(0U,8U)) { const auto p=math::multiply(clip,Vec4{c&1 ? -2.0F : -2.2F,c&2 ? 3.0F : 0,c&4 ? 2.0F : -2.0F,1}); if (p.x>=-p.w) return false; }
    auto settings=r.giSettings(); settings.enabled=false;
    if (!r.setGiSettings(settings) || !test::renderFrames(engine,2)) return false;
    const auto off=measure(engine,Vec3{0},"procedural-off"); settings.enabled=true;
    if (!r.setGiSettings(settings) || !test::renderFrames(engine)) return false;
    const auto on=measure(engine,Vec3{0},"procedural-red");
    if (!r.captureFrameToPng(reports/"gi06a-captures"/"procedural-red.png")) return false;
    g->boxes[0].color=LinearColor{green}; r.notifyNativeDrawChanged();
    if (!test::renderFrames(engine)) return false;
    const auto recolored=measure(engine,Vec3{0},"procedural-green");
    if (!r.captureFrameToPng(reports/"gi06a-captures"/"procedural-green.png")) return false;
    g->boxes.clear(); r.notifyNativeDrawChanged();
    if (!test::renderFrames(engine)) return false;
    const auto removed=measure(engine,Vec3{0},"procedural-removed");
    if (!off || !on || !recolored || !removed) return false;
    const auto a=math::subtract(*on,*off),b=math::subtract(*recolored,*off); const auto error=test::rgbDelta(*off,*removed);
    std::println("native.wall_outside_frustum=1 bounce.red={} bounce.red_green={} bounce.green={} bounce.green_red={} bounce.minimum=0.005 bounce.dominance=1.5 removal.delta={} removal.limit=0.003",a.x,a.x/std::max(a.y,.000001F),b.y,b.y/std::max(b.x,.000001F),error);
    return a.x>.005F && a.x>1.5F*a.y && b.y>.005F && b.y>1.5F*b.x && error<.003F && g->valid;
}
/// @brief A procedural receiver is shaded by deferred GI; a forward surface uses the same public sampler over black depth.
bool receive(simple::Engine &engine,bool forward) {
    auto &r=engine.renderSystem(); auto *g=install(r,forward); if (!g) return false;
    const float y=forward ? .2F : 0.0F;
    g->boxes.push_back(box(Vec3{-.7F,y-.02F,-.7F},Vec3{.7F,y,.7F},Vec3{.5F}));
    const auto wall=r.addCuboid(Vec3{-2.2F,0,-2},Vec3{-2,3,2},LinearColor{red});
    if (!wall) return false;
    if (forward) { const auto black=r.addPlane(Vec2{1,1},LinearColor{Vec3{0}}); if (!black || !r.setObjectUnlit(*black,true)) return false; }
    auto settings=r.giSettings(); settings.enabled=false;
    if (!r.setGiSettings(settings) || !test::renderFrames(engine,2)) return false;
    const Vec3 point{0,y,0}; const auto off=measure(engine,point,"native-receiver-off");
    settings.enabled=true; if (!r.setGiSettings(settings) || !test::renderFrames(engine)) return false;
    const auto redValue=measure(engine,point,"native-receiver-red");
    if (!r.captureFrameToPng(reports/"gi06a-captures"/(forward ? "forward-red.png" : "receive-red.png"))) return false;
    float alphaError{};
    if (forward) {
        g->opacity=1; if (!test::renderFrames(engine,2)) return false;
        const auto full=measure(engine,point,"native-forward-opacity-one");
        if (!full || !redValue) return false; alphaError=test::rgbDelta(math::scale(*full,.5F),*redValue); g->opacity=.5F;
    }
    if (!r.setObjectColor(*wall,LinearColor{green}) || !test::renderFrames(engine)) return false;
    const auto greenValue=measure(engine,point,"native-receiver-green");
    if (!off || !redValue || !greenValue) return false;
    const auto a=math::subtract(*redValue,*off),b=math::subtract(*greenValue,*off);
    std::println("receive.forward={} indirect_only.red={},{},{} indirect_only.green={},{},{} receive.minimum=0.003 receive.dominance=1.5 forward.off={} forward.alpha_error={} forward.alpha_limit=0.001",forward,a.x,a.y,a.z,b.x,b.y,b.z,off->x,alphaError);
    return a.x>.003F && a.x>1.5F*a.y && b.y>.003F && b.y>1.5F*b.x && (!forward || (off->x==0 && off->y==0 && off->z==0 && alphaError<.001F)) && g->valid;
}
/// @brief Reproduce HybridGiLeak's visibility-disabled reference and 70% cross-wall rejection using a procedural blocker.
bool blocker(simple::Engine &engine) {
    auto &r=engine.renderSystem(); auto &renderer=r.forward(); auto *g=install(r); if (!g) return false;
    g->boxes.push_back(box(Vec3{-1.1F,-3,-5},Vec3{-.9F,4,5},Vec3{0}));
    const auto wall=r.addCuboid(Vec3{-3.2F,0,-4},Vec3{-3,3,4},LinearColor{red});
    if (!wall || !r.addPlane(Vec2{8,8},LinearColor{Vec3{.5F}})) return false;
    const Vec3 point{-.5F,0,1},openPoint{-1.6F,0,1};
    const auto camera=Camera::lookAt(Position{Vec3{.2F,1.5F,3}},Position{point}, {},FovY{1.1F}); r.setCamera(camera);
    renderer.debugProbeVisibility=false;
    if (!test::renderFrames(engine)) return false;
    const auto baseline=measure(engine,point,"blocker-visibility-disabled"); renderer.debugProbeVisibility=true;
    if (!test::renderFrames(engine,2)) return false;
    const auto protectedValue=measure(engine,point,"blocker-protected");
    if (!r.captureFrameToPng(reports/"gi06a-captures"/"procedural-blocker.png")) return false;
    r.setCamera(Camera::lookAt(Position{Vec3{-1.7F,1.5F,3}},Position{openPoint}, {},FovY{1.1F}));
    if (!test::renderFrames(engine)) return false;
    const auto open=measure(engine,openPoint,"blocker-open-red");
    if (!r.setObjectColor(*wall,LinearColor{Vec3{0}}) || !test::renderFrames(engine)) return false;
    const auto openZero=measure(engine,openPoint,"blocker-open-black"); r.setCamera(camera);
    if (!test::renderFrames(engine)) return false;
    const auto zero=measure(engine,point,"blocker-black"); renderer.debugProbeVisibility=false;
    if (!test::renderFrames(engine,2)) return false;
    const auto baselineZero=measure(engine,point,"blocker-black-visibility-disabled"); renderer.debugProbeVisibility=true;
    if (!baseline || !protectedValue || !open || !openZero || !zero || !baselineZero) return false;
    const float leak=baseline->x-baselineZero->x,bounce=protectedValue->x-zero->x,openBounce=open->x-openZero->x;
    std::println("native.leak.baseline={} native.leak.protected={} native.leak.retained_fraction={} native.leak.limit=0.30 native.leak.open={} native.leak.open.minimum=0.005",leak,bounce,bounce/std::max(leak,.000001F),openBounce);
    return leak>.005F && bounce<.30F*leak && openBounce>.005F && open->x>1.5F*open->y && g->valid;
}
/// @brief A procedural roof shades an engine wall in both the main view and the independent off-camera capture sun map.
bool shadow(simple::Engine &engine) {
    auto &r=engine.renderSystem(); auto &renderer=r.forward(); auto *g=install(r); if (!g) return false;
    r.setDirectionalLight(Direction{Vec3{-1,-.5F,0}},LinearColor{},LightIntensity{4},LinearColor{Vec3{0}});
    const auto wall=r.addCuboid(Vec3{-2.2F,0,-2},Vec3{-2,3,2},LinearColor{red});
    if (!wall || !r.addPlane(Vec2{1,1},LinearColor{Vec3{.5F}}) || !r.setShadowDistance(1)) return false;
    const auto roof=box(Vec3{-2.3F,3.1F,-3},Vec3{5,3.3F,3},Vec3{0});
    const Vec3 point{0,0,.5F}; r.setCamera(Camera::lookAt(Position{Vec3{0,1.5F,3}},Position{point}, {},FovY{1.1F}));
    if (!test::renderFrames(engine)) return false;
    auto &target=renderer.targets.front(); const auto before=target.probeShadowRenders;
    const auto unroofed=measure(engine,point,"native-capture-unroofed");
    const auto idleBefore=renderer.deviceWaitIdleCount(),meshBefore=renderer.gpuMeshUploadCount(),materialsBefore=renderer.gpuMaterialUploadCount();
    if (!test::renderFrames(engine,4)) return false;
    const auto stable=target.probeShadowRenders-before;
    g->boxes.push_back(roof); r.notifyNativeDrawChanged();
    if (!test::renderFrames(engine,1)) return false;
    const auto notified=target.probeShadowRenders-before;
    if (!test::renderFrames(engine,63)) return false;
    const auto total=target.probeShadowRenders-before;
    const auto roofed=measure(engine,point,"native-capture-roofed");
    const auto idleDelta=renderer.deviceWaitIdleCount()-idleBefore,meshDelta=renderer.gpuMeshUploadCount()-meshBefore,materialDelta=renderer.gpuMaterialUploadCount()-materialsBefore;
    if (!r.captureFrameToPng(reports/"gi06a-captures"/"procedural-shadow.png")) return false;
    // Direct-only main-wall measurements derive the capture threshold, including VVE's existing 0.35 shadow floor.
    auto settings=r.giSettings(); settings.enabled=false;
    if (!r.setGiSettings(settings) || !r.setShadowDistance(60)) return false;
    const Vec3 wallPoint{-2,1.5F,0}; r.setCamera(Camera::lookAt(Position{Vec3{1,1.5F,3}},Position{wallPoint}, {},FovY{1.1F}));
    g->boxes.clear(); r.notifyNativeDrawChanged();
    if (!test::renderFrames(engine,2)) return false;
    const auto direct=measure(engine,wallPoint,"native-main-unroofed");
    g->boxes.push_back(roof); r.notifyNativeDrawChanged();
    if (!test::renderFrames(engine,2)) return false;
    const auto shaded=measure(engine,wallPoint,"native-main-roofed");
    if (!unroofed || !roofed || !direct || !shaded) return false;
    const float reference=(shaded->x-shaded->y)/(direct->x-direct->y),fraction=(roofed->x-roofed->y)/(unroofed->x-unroofed->y),limit=reference+.15F;
    std::println("native.shadow.main_fraction={} native.shadow.capture_fraction={} native.shadow.limit={} native.shadow.steady_refreshes={} native.shadow.notified_refreshes={} native.shadow.converged_refreshes={} native.shadow.calls={}",reference,fraction,limit,stable,notified,total,g->sunCalls);
    std::println("native.notification.idle_waits={} native.notification.mesh_uploads={} native.notification.material_uploads={}",idleDelta,meshDelta,materialDelta);
    return unroofed->x-unroofed->y>.005F && reference<.5F && fraction<limit && limit<.65F && stable==0 && notified==1 && total==1 && idleDelta==0 && meshDelta==0 && materialDelta==0 && g->valid;
}
/// @brief Preserve the exact pre-hook output for all backends and coverage counts; compare whole PNG bytes later.
bool noneRegistered(bool save) {
    std::filesystem::create_directories(reports/"gi06a-baseline");
    // Fresh checkouts have no driver-specific historical PNGs: first render a clean control, then compare an explicit clear.
    if (!save && !std::filesystem::exists(reports/"gi06a-baseline"/"forward-1.png")) {
        std::println("baseline.provenance=current-control (use save-baseline before editing for pre-change evidence)");
        if (!noneRegistered(true)) return false;
    }
    bool pass=true;
    for (const auto backend : {RendererBackend::forward,RendererBackend::deferred,RendererBackend::hybrid_gi}) {
        for (const auto aa : {Antialiasing::none,Antialiasing::msaa4}) {
            auto engine=test::hiddenEngine("native-baseline",PixelExtent{128,128}); auto &r=engine.renderSystem();
            if (!save && !r.setNativeDraw({})) return false;
            if (!engine.init() || !r.setRendererBackend(backend) || !r.setAntialiasing(aa)) return false;
            r.setCamera(Camera::lookAt(Position{Vec3{0,1.5F,3}},Position{}, {},FovY{1.1F}));
            r.setBackgroundColor(LinearColor{Vec3{.02F,.04F,.07F}});
            r.setDirectionalLight(Direction{Vec3{-1,-.5F,0}},LinearColor{},LightIntensity{4},LinearColor{Vec3{0}});
            if (!r.addPlane(Vec2{2,2},LinearColor{Vec3{.5F}}) || !r.addCuboid(Vec3{-2.2F,0,-2},Vec3{-2,3,2},LinearColor{Vec3{1,.01F,.01F}})) return false;
            const auto glass=r.addCuboid(Vec3{-.4F,0,-.3F},Vec3{.3F,.8F,.3F},LinearColor{Vec3{.05F,.2F,.7F}});
            if (!glass || !r.setObjectOpacity(*glass,.4F) || !test::renderFrames(engine)) return false;
            const auto name=std::format("{}-{}.png",std::array{"forward","deferred","hybrid-gi"}[std::to_underlying(backend)],aa==Antialiasing::none ? 1 : 4);
            const auto baseline=reports/"gi06a-baseline"/name, current=reports/"gi06a-captures"/name;
            std::filesystem::create_directories(current.parent_path());
            if (!r.captureFrameToPng(save ? baseline : current)) return false;
            const auto bytes=[](const auto &path) { std::ifstream file{path,std::ios::binary}; return std::string{std::istreambuf_iterator<char>{file},{}}; };
            const bool same=save || (!bytes(baseline).empty() && bytes(baseline)==bytes(current));
            VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(r.forward().physicalDevice.physicalDevice,&device);
            pass &= same && requiredValidation(r.forward());
            r.waitIdle(); engine.gui().shutdownVulkan(); r.shutdown();
            pass &= r.forward().validationErrorCount()==0;
            std::println("baseline={} saved={} byte_identical={} device={} validation.errors={}",name,save,same,std::string_view{device.deviceName},r.forward().validationErrorCount());
        }
    }
    return pass;
}
}
/// @brief Registered native Vulkan subcommands; existing simple setup/readback hooks provide only diagnostics.
int main(int argc,char **argv) {
    const std::string_view mode=argc==1 ? "passes" : argv[1];
    const std::array modes{"passes","procedural-bounce","procedural-blocker","procedural-shadow","receive","forward-sample","none-registered","save-baseline"};
    if (argc>2 || std::ranges::find(modes,mode)==modes.end()) return 3;
    std::filesystem::create_directories(reports/"gi06a-captures");
    if (mode=="save-baseline" || mode=="none-registered") return noneRegistered(mode=="save-baseline") ? 0 : 2;
    if (mode=="passes" && !passes()) return 2;
    auto engine=test::hiddenEngine("native-gi",PixelExtent{128,128}); auto &r=engine.renderSystem();
    if (!engine.init() || !r.setRendererBackend(RendererBackend::hybrid_gi) || !r.setAnimationTime(AnimationTime{})) return 1;
    lighting(engine);
    bool pass=mode=="procedural-bounce" ? bounce(engine) : mode=="procedural-blocker" ? blocker(engine) : mode=="procedural-shadow" ? shadow(engine) : receive(engine,mode=="forward-sample");
    auto &renderer=r.forward(); VkPhysicalDeviceProperties device{}; vkGetPhysicalDeviceProperties(renderer.physicalDevice.physicalDevice,&device);
    pass &= requiredValidation(renderer);
    std::println("native.mode={} device={} validation_active={} test.pass={}",mode,std::string_view{device.deviceName},renderer.validationActive(),pass);
    if (!r.setNativeDraw({})) return 1;
    r.waitIdle(); engine.gui().shutdownVulkan(); r.shutdown();
    std::println("validation.errors={}",renderer.validationErrorCount());
    return pass && renderer.validationErrorCount()==0 ? 0 : 2;
}
