/* This implementation is heavily "inspired" by the official imgui_impl_vulkan.h */
#include "imgui_impl_slrd.h"
#include "slrd/commandbuffer.hpp"
#include "slrd/commandqueue.hpp"
#include "slrd/fence.hpp"
#include "slrd/uniformupdater.hpp"
#include "slrd/device.hpp"
#include "slrd/pipeline.hpp"
#include "slrd/sampler.hpp"
#include "slrd/shader.hpp"
#include <forward_list>
#include <array>

/* shader code */
static uint32_t s_glslVertex[] = {
	// 1115.1.0
	0x07230203,0x00010000,0x0008000b,0x00000031,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x000a000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000b,0x0000000f,0x00000015,
	0x0000001e,0x0000001f,0x00030003,0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00030005,0x00000009,0x00000000,0x00050006,0x00000009,0x00000000,0x6f6c6f63,
	0x00000072,0x00060006,0x00000009,0x00000001,0x43786574,0x64726f6f,0x00000073,0x00060005,
	0x0000000b,0x4174756f,0x69727474,0x65747562,0x00000073,0x00040005,0x0000000f,0x6c6f4361,
	0x0000726f,0x00030005,0x00000015,0x00565561,0x00060005,0x0000001c,0x505f6c67,0x65567265,
	0x78657472,0x00000000,0x00060006,0x0000001c,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,
	0x00070006,0x0000001c,0x00000001,0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,
	0x0000001c,0x00000002,0x435f6c67,0x4470696c,0x61747369,0x0065636e,0x00070006,0x0000001c,
	0x00000003,0x435f6c67,0x446c6c75,0x61747369,0x0065636e,0x00030005,0x0000001e,0x00000000,
	0x00050005,0x0000001f,0x736f5061,0x6f697469,0x0000006e,0x00060005,0x00000021,0x73755075,
	0x6e6f4368,0x6e617473,0x00000074,0x00050006,0x00000021,0x00000000,0x6c616373,0x00000065,
	0x00060006,0x00000021,0x00000001,0x6e617274,0x74616c73,0x006e6f69,0x00030005,0x00000023,
	0x00006370,0x00040047,0x0000000b,0x0000001e,0x00000000,0x00040047,0x0000000f,0x0000001e,
	0x00000002,0x00040047,0x00000015,0x0000001e,0x00000001,0x00030047,0x0000001c,0x00000002,
	0x00050048,0x0000001c,0x00000000,0x0000000b,0x00000000,0x00050048,0x0000001c,0x00000001,
	0x0000000b,0x00000001,0x00050048,0x0000001c,0x00000002,0x0000000b,0x00000003,0x00050048,
	0x0000001c,0x00000003,0x0000000b,0x00000004,0x00040047,0x0000001f,0x0000001e,0x00000000,
	0x00030047,0x00000021,0x00000002,0x00050048,0x00000021,0x00000000,0x00000023,0x00000000,
	0x00050048,0x00000021,0x00000001,0x00000023,0x00000008,0x00020013,0x00000002,0x00030021,
	0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,
	0x00000004,0x00040017,0x00000008,0x00000006,0x00000002,0x0004001e,0x00000009,0x00000007,
	0x00000008,0x00040020,0x0000000a,0x00000003,0x00000009,0x0004003b,0x0000000a,0x0000000b,
	0x00000003,0x00040015,0x0000000c,0x00000020,0x00000001,0x0004002b,0x0000000c,0x0000000d,
	0x00000000,0x00040020,0x0000000e,0x00000001,0x00000007,0x0004003b,0x0000000e,0x0000000f,
	0x00000001,0x00040020,0x00000011,0x00000003,0x00000007,0x0004002b,0x0000000c,0x00000013,
	0x00000001,0x00040020,0x00000014,0x00000001,0x00000008,0x0004003b,0x00000014,0x00000015,
	0x00000001,0x00040020,0x00000017,0x00000003,0x00000008,0x00040015,0x00000019,0x00000020,
	0x00000000,0x0004002b,0x00000019,0x0000001a,0x00000001,0x0004001c,0x0000001b,0x00000006,
	0x0000001a,0x0006001e,0x0000001c,0x00000007,0x00000006,0x0000001b,0x0000001b,0x00040020,
	0x0000001d,0x00000003,0x0000001c,0x0004003b,0x0000001d,0x0000001e,0x00000003,0x0004003b,
	0x00000014,0x0000001f,0x00000001,0x0004001e,0x00000021,0x00000008,0x00000008,0x00040020,
	0x00000022,0x00000009,0x00000021,0x0004003b,0x00000022,0x00000023,0x00000009,0x00040020,
	0x00000024,0x00000009,0x00000008,0x0004002b,0x00000006,0x0000002b,0x00000000,0x0004002b,
	0x00000006,0x0000002c,0x3f800000,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,
	0x000200f8,0x00000005,0x0004003d,0x00000007,0x00000010,0x0000000f,0x00050041,0x00000011,
	0x00000012,0x0000000b,0x0000000d,0x0003003e,0x00000012,0x00000010,0x0004003d,0x00000008,
	0x00000016,0x00000015,0x00050041,0x00000017,0x00000018,0x0000000b,0x00000013,0x0003003e,
	0x00000018,0x00000016,0x0004003d,0x00000008,0x00000020,0x0000001f,0x00050041,0x00000024,
	0x00000025,0x00000023,0x0000000d,0x0004003d,0x00000008,0x00000026,0x00000025,0x00050085,
	0x00000008,0x00000027,0x00000020,0x00000026,0x00050041,0x00000024,0x00000028,0x00000023,
	0x00000013,0x0004003d,0x00000008,0x00000029,0x00000028,0x00050081,0x00000008,0x0000002a,
	0x00000027,0x00000029,0x00050051,0x00000006,0x0000002d,0x0000002a,0x00000000,0x00050051,
	0x00000006,0x0000002e,0x0000002a,0x00000001,0x00070050,0x00000007,0x0000002f,0x0000002d,
	0x0000002e,0x0000002b,0x0000002c,0x00050041,0x00000011,0x00000030,0x0000001e,0x0000000d,
	0x0003003e,0x00000030,0x0000002f,0x000100fd,0x00010038
};

static uint32_t s_glslFrag[] = {
	// 1115.1.0
	0x07230203,0x00010000,0x0008000b,0x0000001e,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000d,0x00030010,
	0x00000004,0x00000007,0x00030003,0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00050005,0x00000009,0x4374756f,0x726f6c6f,0x00000000,0x00030005,0x0000000b,
	0x00000000,0x00050006,0x0000000b,0x00000000,0x6f6c6f63,0x00000072,0x00060006,0x0000000b,
	0x00000001,0x43786574,0x64726f6f,0x00000073,0x00050005,0x0000000d,0x74744169,0x75626972,
	0x00736574,0x00050005,0x00000016,0x78655475,0x65727574,0x00000000,0x00040047,0x00000009,
	0x0000001e,0x00000000,0x00040047,0x0000000d,0x0000001e,0x00000000,0x00040047,0x00000016,
	0x00000021,0x00000000,0x00040047,0x00000016,0x00000022,0x00000000,0x00020013,0x00000002,
	0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,
	0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,
	0x00000009,0x00000003,0x00040017,0x0000000a,0x00000006,0x00000002,0x0004001e,0x0000000b,
	0x00000007,0x0000000a,0x00040020,0x0000000c,0x00000001,0x0000000b,0x0004003b,0x0000000c,
	0x0000000d,0x00000001,0x00040015,0x0000000e,0x00000020,0x00000001,0x0004002b,0x0000000e,
	0x0000000f,0x00000000,0x00040020,0x00000010,0x00000001,0x00000007,0x00090019,0x00000013,
	0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x0003001b,
	0x00000014,0x00000013,0x00040020,0x00000015,0x00000000,0x00000014,0x0004003b,0x00000015,
	0x00000016,0x00000000,0x0004002b,0x0000000e,0x00000018,0x00000001,0x00040020,0x00000019,
	0x00000001,0x0000000a,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
	0x00000005,0x00050041,0x00000010,0x00000011,0x0000000d,0x0000000f,0x0004003d,0x00000007,
	0x00000012,0x00000011,0x0004003d,0x00000014,0x00000017,0x00000016,0x00050041,0x00000019,
	0x0000001a,0x0000000d,0x00000018,0x0004003d,0x0000000a,0x0000001b,0x0000001a,0x00050057,
	0x00000007,0x0000001c,0x00000017,0x0000001b,0x00050085,0x00000007,0x0000001d,0x00000012,
	0x0000001c,0x0003003e,0x00000009,0x0000001d,0x000100fd,0x00010038
};

static void ImGui_ImplSLRD_UpdateTexture (ImTextureData *texture);

static inline const auto& ImGui_ImplSLRD_VertexBinding () {
    static const std::array<slrd::VertexBindingDescription, 1> bindings = {
        slrd::VertexBindingDescription {
            .binding = 0,
            .stride = sizeof (ImDrawVert),
        }
    };

    return bindings;
}

static inline const auto& ImGui_ImplSLRD_VertexAttribute () {
    static const std::array<slrd::VertexAttributeDescription, 3> attributes = {
        slrd::VertexAttributeDescription {
            .location = 0,
            .binding = 0,
            .offset  = 0,
            .format  = slrd::FORMAT_RG32_SFLOAT,
        },
        slrd::VertexAttributeDescription {
            .location = 1,
            .binding  = 0,
            .offset   = offsetof (ImDrawVert, uv),
            .format   = slrd::FORMAT_RG32_SFLOAT
        },
        slrd::VertexAttributeDescription {
            .location = 2,
            .binding  = 0,
            .offset   = offsetof (ImDrawVert, col),
            .format   = slrd::FORMAT_RGBA8_UNORM
        }
    };

    return attributes;
}

struct ImGui_ImplSLRD_PushConstant {
    ImVec2 scale;
    ImVec2 translation;
};

struct ImGui_ImplSLRD_Frame {
    slrd::CommandBufferPtr cmd_buffer;

    slrd::BufferPtr idx_buffer;
    slrd::BufferPtr vtx_buffer;

    uint32_t vtx_size;
    uint32_t idx_size;
};

struct ImGui_ImplSLRD_Texture {
    /* Uniform set alloated from the pipeline to draw the texture in a shader */
    slrd::UniformSetPtr set;
    /* The texture data */
    slrd::TexturePtr texture;
    /* The texture view for the texture data */
    slrd::TextureViewPtr texture_view;
};

struct ImGui_ImplSLRD_Data {
    /* Device */
    slrd::DevicePtr device;

    /* The main pipeline */
    slrd::PipelinePtr pipeline;
    /* The queue used for transitions */
    slrd::CommandQueuePtr queue;

    /* Sampler for the textures */
    slrd::SamplerPtr sampler;

    /* Fence for submit operations */
    slrd::FencePtr fence;

    uint32_t current_frame;

    /* Assigned textures */
    std::forward_list<ImGui_ImplSLRD_Texture> textures;

    /* Frames in flight */
    ImVector<ImGui_ImplSLRD_Frame> frames;
};

static inline ImGui_ImplSLRD_Data *ImGui_ImplSLRD_GetBackendData () {
    return ImGui::GetCurrentContext() ?
        (ImGui_ImplSLRD_Data *)ImGui::GetIO().BackendRendererUserData :
        nullptr;
}

static inline bool ImGui_ImplSLRD_InitState () {
    auto* backend_data = ImGui_ImplSLRD_GetBackendData ();

    slrd::PipelinePtr pipeline;
    slrd::ShaderPtr   shader;
    slrd::SamplerPtr  sampler;
    slrd::FencePtr    fence;

    slrd::ShaderBytecode bytecodes[] = {
        s_glslVertex,
        s_glslFrag
    };

    slrd::ShaderInfo shader_info;
    shader_info.bytecodes = bytecodes;

    shader = backend_data->device->createShader (shader_info);
    if (!shader) {
        return false;
    }

    slrd::ColorBlendAttachment colors[1];
    colors[0].blendEnabled = true;
    colors[0].srcColorBlendFactor = slrd::BLEND_FACTOR_SRC_ALPHA;
    colors[0].dstColorBlendFactor = slrd::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colors[0].colorBlendOperation = slrd::BLEND_OPERATION_ADD;
    colors[0].srcAlphaBlendFactor = slrd::BLEND_FACTOR_ONE;
    colors[0].dstAlphaBlendFactor = slrd::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colors[0].alphaBlendOperation = slrd::BLEND_OPERATION_ADD;
    colors[0].colorWriteMask = slrd::COLOR_MASK_RGBA;

    slrd::GraphicsPipelineInfo pipeline_info;
    pipeline_info.shader = std::move (shader);
    pipeline_info.vertexConfig.vertexBindings = ImGui_ImplSLRD_VertexBinding ();
    pipeline_info.vertexConfig.attributeDescs = ImGui_ImplSLRD_VertexAttribute ();
    pipeline_info.colorBlendConfig.attachments = colors;
    pipeline_info.rasterizerConfig.cullMode = slrd::CULL_MODE_NONE;

    pipeline = backend_data->device->createGraphicsPipeline (pipeline_info);
    if (!pipeline) {
        return false;
    }
    
    slrd::SamplerInfo sampler_info;
    sampler_info.minFilter = slrd::FILTER_LINEAR;
    sampler_info.magFilter = slrd::FILTER_LINEAR;
    sampler_info.anisotropy = false;
    sampler_info.maxLod = -1000.f;
    sampler_info.minLod = -1000.f;
    sampler_info.addressModeU = slrd::SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = slrd::SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = slrd::SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    sampler = backend_data->device->createSampler (sampler_info);
    if (!sampler) {
        return false;
    }

    fence = backend_data->device->createFence ();
    if (!fence) {
        return false;
    }

    backend_data->pipeline = std::move (pipeline);
    backend_data->sampler  = std::move (sampler);
    backend_data->fence    = std::move (fence);

    return true;
}

IMGUI_IMPL_API bool ImGui_ImplSLRD_Init (ImGui_ImplSLRD_InitInfo* info) {
    IM_ASSERT (info != nullptr);

    ImGuiIO& io = ImGui::GetIO ();
    IMGUI_CHECKVERSION ();
    IM_ASSERT (io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    ImGui_ImplSLRD_Data* backend_data = IM_NEW (ImGui_ImplSLRD_Data)();

    io.BackendRendererUserData = (void *)backend_data;
    io.BackendRendererName = "imgui_impl_slrd";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    IM_ASSERT (info->device != nullptr);
    IM_ASSERT (info->command_queue != nullptr);
    IM_ASSERT (info->frames >=1);
    
    backend_data->device = std::move (info->device);
    backend_data->queue = std::move (info->command_queue);
    backend_data->frames.resize (info->frames, {});
    backend_data->current_frame = 0;

    if (!ImGui_ImplSLRD_InitState ()) {
        io.BackendRendererUserData = nullptr;
        IM_DELETE (backend_data);

        return false;
    }

    return true;
}

IMGUI_IMPL_API void ImGui_ImplSLRD_Shutdown () {
    ImGui_ImplSLRD_Data* backend_data = ImGui_ImplSLRD_GetBackendData ();
    IM_ASSERT(backend_data != nullptr &&
            "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasTextures;

    IM_DELETE (backend_data);
}

IMGUI_IMPL_API void ImGui_ImplSLRD_NewFrame () {
    ImGui_ImplSLRD_Data* backend_data = ImGui_ImplSLRD_GetBackendData ();
    IM_ASSERT(backend_data != nullptr &&
            "No renderer backend to draw with, or already shutdown?");

    backend_data->current_frame = (backend_data->current_frame + 1) %
        backend_data->frames.size ();
}

IMGUI_IMPL_API void ImGui_ImplSLRD_RenderDrawData (ImDrawData* draw_data, slrd::CommandBufferPtr& command_buffer) {
    int fb_width  = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);

    if (fb_width <= 0 || fb_height <= 0)
        return;

    ImGui_ImplSLRD_Data* backend_data = ImGui_ImplSLRD_GetBackendData ();
    IM_ASSERT(backend_data != nullptr &&
            "No renderer backend to draw with, or already shutdown?");

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGui_ImplSLRD_UpdateTexture (tex);

    // Setup orthographic projection matrix cover draw_data->DisplayPos to draw_data->DisplayPos + draw_data->DisplaySize
    // Setup viewport covering draw_data->DisplayPos to draw_data->DisplayPos + draw_data->DisplaySize
    command_buffer->bindGraphicsPipeline (backend_data->pipeline);
    command_buffer->setViewport ({
            draw_data->DisplayPos.x, draw_data->DisplayPos.y,
            draw_data->DisplaySize.x, draw_data->DisplaySize.y });

    ImGui_ImplSLRD_PushConstant pc;
    pc.scale.x = 2.0f / draw_data->DisplaySize.x;
    pc.scale.y = 2.0f / draw_data->DisplaySize.y;
    pc.translation.x = -1.0f - draw_data->DisplayPos.x * pc.scale.x;
    pc.translation.y = -1.0f - draw_data->DisplayPos.y * pc.scale.y;
    command_buffer->pushConstant ({ (uint8_t *)&pc, sizeof (pc) }, slrd::STAGE_VERTEX);

    ImGui_ImplSLRD_Frame& frame = backend_data->frames[backend_data->current_frame];
    frame.cmd_buffer = command_buffer;

    /* Do the same thing as in the vulkan backend - upload a single huge buffer */
    if (draw_data->TotalVtxCount > 0) {
        size_t vtx_size = draw_data->TotalVtxCount * sizeof (ImDrawVert);
        size_t idx_size = draw_data->TotalIdxCount * sizeof (ImDrawIdx);

        if (!frame.vtx_buffer || vtx_size > frame.vtx_size) {
            slrd::BufferPtr buffer;

            slrd::BufferInfo buffer_info;
            buffer_info.gpu = true;
            buffer_info.coherent = true;
            buffer_info.size = vtx_size;
            buffer_info.usage = slrd::BUFFER_USAGE_VERTEX_BUFFER;
            buffer_info.properties = slrd::BUFFER_PROPERTY_TRANSFER_DST;

            buffer = backend_data->device->createBuffer (buffer_info);
            IM_ASSERT (buffer && "Couldn't create a vertex buffer");

            frame.vtx_size   = vtx_size;
            frame.vtx_buffer = std::move (buffer);
        }

        if (!frame.idx_buffer || idx_size > frame.idx_size) {
            slrd::BufferPtr buffer;

            slrd::BufferInfo buffer_info;
            buffer_info.gpu = true;
            buffer_info.coherent = true;
            buffer_info.size = idx_size;
            buffer_info.usage = slrd::BUFFER_USAGE_INDEX_BUFFER;
            buffer_info.properties = slrd::BUFFER_PROPERTY_TRANSFER_DST;

            buffer = backend_data->device->createBuffer (buffer_info);
            IM_ASSERT (buffer && "Couldn't create an index buffer");

            frame.idx_size   = idx_size;
            frame.idx_buffer = std::move (buffer);
        }

        ImDrawVert *vtx_map = (ImDrawVert *)frame.vtx_buffer->map ();
        IM_ASSERT (vtx_map && "Couldn't map the buffer");
        ImDrawIdx *idx_map = (ImDrawIdx *)frame.idx_buffer->map ();
        IM_ASSERT (idx_map && "Couldn't map the buffer");

        for (int n = 0; n < draw_data->CmdListsCount; n++) {
            const ImDrawList *cmd_list = draw_data->CmdLists[n];
            const ImDrawVert *vtx_buffer = cmd_list->VtxBuffer.Data;  // vertex buffer generated by Dear ImGui
            const ImDrawIdx *idx_buffer = cmd_list->IdxBuffer.Data;   // index buffer generated by Dear ImGui

            ::memcpy (vtx_map, vtx_buffer, cmd_list->VtxBuffer.size () * sizeof (ImDrawVert));
            ::memcpy (idx_map, idx_buffer, cmd_list->IdxBuffer.size () * sizeof (ImDrawIdx));

            vtx_map += cmd_list->VtxBuffer.size ();
            idx_map += cmd_list->IdxBuffer.size ();
        }

        frame.vtx_buffer->unmap ();
        frame.idx_buffer->unmap ();
    }

    uint32_t global_vtx_offset = 0;
    uint32_t global_idx_offset = 0;

    // Render command lists
    ImVec2 clip_off = draw_data->DisplayPos;
    ImVec2 clip_scale = draw_data->FramebufferScale;
    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback) {
                /*if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)*/
                /*    MyEngineSetupenderState();*/
                /*else*/

                pcmd->UserCallback(cmd_list, pcmd);
            } else {
                // Project scissor/clipping rectangles into framebuffer space
                // - Clipping coordinates are provided in imgui coordinates space:
                //   - For a given viewport, draw_data->DisplayPos == viewport->Pos and draw_data->DisplaySize == viewport->Size
                //   - In a single viewport application, draw_data->DisplayPos == (0,0) and draw_data->DisplaySize == io.DisplaySize, but always use GetMainViewport()->Pos/Size instead of hardcoding those values.
                //   - In the interest of supporting multi-viewport applications (see 'docking' branch on github),
                //     always subtract draw_data->DisplayPos from clipping bounds to convert them to your viewport space.
                // - Note that pcmd->ClipRect contains Min+Max bounds. Some graphics API may use Min+Max, other may use Min+Size (size being Max-Min)
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f)
                    clip_min.x = 0.0f;
                if (clip_min.y < 0.0f)
                    clip_min.y = 0.0f;

                if (clip_max.x > fb_width)
                    clip_max.x = (float)fb_width;
                if (clip_max.y > fb_height)
                    clip_max.y = (float)fb_height;

                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                slrd::Scissor scissor;
                scissor.x = (int32_t)clip_min.x;
                scissor.y = (int32_t)clip_min.y;
                scissor.w = (uint32_t)(clip_max.x - clip_min.x);
                scissor.h = (uint32_t)(clip_max.y - clip_min.y);
                command_buffer->setScissor (scissor);

                // The texture for the draw call is specified by pcmd->GetTexID().
                // The vast majority of draw calls will use the Dear ImGui texture atlas, which value you have set yourself during initialization.
                slrd::UniformSetPtr *texture = (slrd::UniformSetPtr *)pcmd->GetTexID ();
                slrd::IUniformSet *sets[] = {
                    texture->get ()
                };
                command_buffer->bindSets (sets);

                slrd::IndexType index_type = sizeof (ImDrawIdx) == 2 ?
                    slrd::INDEX_TYPE_UINT16 : slrd::INDEX_TYPE_UINT32;
                command_buffer->bindIndexBuffer (frame.idx_buffer, index_type);
                command_buffer->bindVertexBuffer (frame.vtx_buffer, 0);

                command_buffer->drawIndexed (pcmd->ElemCount, 1,
                        global_idx_offset + pcmd->IdxOffset,
                        global_vtx_offset + pcmd->VtxOffset);
            }
        }

        global_vtx_offset += cmd_list->VtxBuffer.size ();
        global_idx_offset += cmd_list->IdxBuffer.size ();
    }
}

static void ImGui_ImplSLRD_UpdateTexture (ImTextureData *tex) {
    auto *backend_data = ImGui_ImplSLRD_GetBackendData ();

    if (tex->Status == ImTextureStatus_WantCreate) {
        // Create texture based on tex->Width, tex->Height.
        // - Most backends only support tex->Format == ImTextureFormat_RGBA32.
        // - Backends for particularly memory constrainted platforms may support tex->Format == ImTextureFormat_Alpha8.

        // Upload all texture pixels
        // - Read from our CPU-side copy of the texture and copy to your graphics API.
        // - Use tex->Width, tex->Height, tex->GetPixels(), tex->GetPixelsAt(), tex->GetPitch() as needed.

        // Store your data, and acknowledge creation.

        slrd::TexturePtr  texture;
        slrd::TextureViewPtr view;
        slrd::UniformSetPtr   set;

        slrd::TextureInfo texture_info;
        texture_info.usage = slrd::TEXTURE_USAGE_SAMPLED | slrd::TEXTURE_USAGE_TRANSFER_DST;
        texture_info.format = slrd::FORMAT_RGBA8_UNORM;
        texture_info.type  = slrd::TEXTURE_TYPE_2D;
        texture_info.width = tex->Width;
        texture_info.height = tex->Height;
        texture_info.depth = 1;

        texture = backend_data->device->createTexture (texture_info);
        IM_ASSERT (texture && "Failed to create a texture!");

        view = texture->createTextureView ({});
        IM_ASSERT (view && "Failed to create a texture view!");

        set = ImGui_ImplSLRD_AddTexture (texture, view, slrd::TEXTURE_LAYOUT_SHADER_READ_ONLY);

        ImGui_ImplSLRD_Texture impl_texture;
        impl_texture.texture = std::move (texture);
        impl_texture.texture_view = std::move (view);
        impl_texture.set = std::move (set);

        auto *user_data = &backend_data->textures.emplace_front (
                    std::move (impl_texture));
        ImTextureID tex_id = (ImTextureID)&user_data->set;

        tex->SetTexID (tex_id); // Specify backend-specific ImTextureID identifier which will be stored in ImDrawCmd.
        tex->BackendUserData = user_data; // Store more backend data if needed (most backend allocate a small texture to store data in there)
    }
    if (tex->Status == ImTextureStatus_WantCreate ||
            tex->Status == ImTextureStatus_WantUpdates) {
        // Upload a rectangle of pixels to the existing texture
        // - We only ever write to textures regions which have never been used before!
        // - Use tex->TexID or tex->BackendUserData to retrieve your stored data.
        // - Use tex->UpdateRect.x/y, tex->UpdateRect.w/h to obtain the block position and size.
        //   - Use tex->Updates[] to obtain individual sub-regions within tex->UpdateRect. Not recommended.
        // - Read from our CPU-side copy of the texture and copy to your graphics API.
        // - Use tex->Width, tex->Height, tex->GetPixels(), tex->GetPixelsAt(), tex->GetPitch() as needed.

        auto *backend_tex = (ImGui_ImplSLRD_Texture *)tex->BackendUserData;

        // Update full texture or selected blocks. We only ever write to textures regions which have never been used before!
        // This backend choose to use tex->UpdateRect but you can use tex->Updates[] to upload individual regions.
        // We could use the smaller rect on _WantCreate but using the full rect allows us to clear the texture.
        const int x = (tex->Status == ImTextureStatus_WantCreate) ? 0 : tex->UpdateRect.x;
        const int y = (tex->Status == ImTextureStatus_WantCreate) ? 0 : tex->UpdateRect.y;
        const int w = (tex->Status == ImTextureStatus_WantCreate) ? tex->Width : tex->UpdateRect.w;
        const int h = (tex->Status == ImTextureStatus_WantCreate) ? tex->Height : tex->UpdateRect.h;

        slrd::BufferPtr staging_buffer;

        const size_t pitch = w * tex->BytesPerPixel;
        const size_t size = h * pitch;

        {
            slrd::BufferInfo buffer_info;
            buffer_info.usage = 0;
            buffer_info.size  = size;
            buffer_info.coherent = true;
            buffer_info.gpu = true;
            buffer_info.properties = slrd::BUFFER_PROPERTY_TRANSFER_SRC;

            staging_buffer = backend_data->device->createBuffer (buffer_info);
            IM_ASSERT (staging_buffer && "Failed to create a staging buffer for texture upload");
        }

        auto *map = (uint8_t *)staging_buffer->map ();
        IM_ASSERT (map && "Failed to map the staging buffer for texture upload");

        for (int i = 0; i < h; ++i) {
            ::memcpy (map + pitch * i, tex->GetPixelsAt (x, y + i), pitch);
        }

        staging_buffer->unmap ();

        slrd::CommandBufferPtr cmd_buffer = backend_data->queue->getCommandBuffer ();
        IM_ASSERT (cmd_buffer && "Failed to create a command buffer");

        cmd_buffer->begin (); {
            slrd::TextureBarrierInfo barrier {};
            barrier.texture = backend_tex->texture.get ();
            barrier.currentTextureLayout = slrd::TEXTURE_LAYOUT_UNDEFINED;
            barrier.newTextureLayout = slrd::TEXTURE_LAYOUT_TRANSFER_DST;
            cmd_buffer->pipelineTextureBarrier (barrier);

            slrd::BufferTextureRegion region {};
            region.rect = { (uint32_t)x, (uint32_t)y, 0, (uint32_t)w, (uint32_t)h, 1 };

            slrd::BufferTextureCopyInfo copy_data;
            copy_data.texture = backend_tex->texture.get ();
            copy_data.buffer  = staging_buffer.get ();
            copy_data.regions = &region;

            cmd_buffer->copyBufferToImage (copy_data);

            barrier.currentTextureLayout = slrd::TEXTURE_LAYOUT_TRANSFER_DST;
            barrier.newTextureLayout = slrd::TEXTURE_LAYOUT_SHADER_READ_ONLY;
            cmd_buffer->pipelineTextureBarrier (barrier);
        } cmd_buffer->end ();

        slrd::SubmitInfo submit_info;
        submit_info.commandBuffers = &cmd_buffer;
        submit_info.fence = backend_data->fence;

        backend_data->queue->submit (submit_info);
        IM_ASSERT (backend_data->fence->wait () == 0 && "Failed waiting on the fence");

        backend_data->fence->reset ();

        // Acknowledge update
        tex->SetStatus(ImTextureStatus_OK);
    }
    if (tex->Status == ImTextureStatus_WantDestroy &&
            tex->UnusedFrames > backend_data->frames.size ()) {
        // If you use staged rendering and have in-flight renders, changed tex->UnusedFrames > 0 check to higher count as needed e.g. > 2

        // Destroy texture
        // - Use tex->TexID or tex->BackendUserData to retrieve your stored data.
        // - Destroy texture in your graphics API.

        // Acknowledge destruction
        auto *backend_tex = (ImGui_ImplSLRD_Texture *)tex->BackendUserData;
        const auto tex_id = tex->TexID;

        // Remove from the linked list
        auto prev = backend_data->textures.before_begin ();
        for (auto it = backend_data->textures.begin ();
                it != backend_data->textures.end (); ++it, prev = it) {
            if (&(it->set) == (slrd::UniformSetPtr *)tex_id) {
                backend_data->textures.erase_after (prev);
                break;
            }
        }

        tex->SetTexID(ImTextureID_Invalid);
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

IMGUI_IMPL_API slrd::UniformSetPtr ImGui_ImplSLRD_AddTexture (slrd::TexturePtr texture,
        slrd::TextureViewPtr view, slrd::TextureLayout layout) {
    ImGui_ImplSLRD_Data* backend_data = ImGui_ImplSLRD_GetBackendData ();
    IM_ASSERT(backend_data != nullptr &&
            "No renderer backend");

    slrd::UniformSetPtr set;
    set = backend_data->pipeline->allocateUniformSet (0);
    IM_ASSERT (set && "Failed to allocate a uniform set!");

    slrd::UniformUpdater updater;
    updater
        .updateCombinedTexture (0, view.get (), backend_data->sampler.get (),
                slrd::TEXTURE_LAYOUT_SHADER_READ_ONLY);

    set->updateUniforms (updater.get ());

    return set;
}

IMGUI_IMPL_API void ImGui_ImplSLRD_RemoveTexture (slrd::UniformSetPtr set) {
    ImGui_ImplSLRD_Data* backend_data = ImGui_ImplSLRD_GetBackendData ();
    IM_ASSERT(backend_data != nullptr &&
            "No renderer backend");
}
