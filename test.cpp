#include <SDL2/SDL.h>

#define STB_IMAGE_IMPLEMENTATION 1
#include "stb/stb_image.h"
#include <SDL_vulkan.h>
#include <filesystem>
#include <iostream>
#include <slrd/slrd.hpp>
#include <slrd/platform/vulkan.hpp>
#include <imgui.h>
#include "backends/imgui_impl_sdl2.h"
#include "imgui_impl_slrd.h"

static SDL_Window *window;
static slrd::DevicePtr device;
static slrd::SwapchainPtr swapchain;
static slrd::RenderPassPtr renderpass;

static slrd::TexturePtr texture;
static slrd::TextureViewPtr texture_view;
static slrd::UniformSetPtr im_texture;

static slrd::CommandQueuePtr queue;
static constexpr uint32_t NR_FIF = 2;

uint32_t current_frame = 0;
static slrd::FencePtr fences[NR_FIF];
static slrd::ICommandBuffer *cmd_buffers[NR_FIF];

static ImGuiContext *imContext;

static slrd::TexturePtr createTextureFromImage (
        const std::filesystem::path& path, slrd::TextureViewPtr *texView);
static void recreateSwapchain ();

int main (void) {
    if (SDL_Init (SDL_INIT_EVENTS)) {
        throw std::runtime_error ("SDL2");
    }

    window = SDL_CreateWindow ("ImGUI test", 0, 0, 800, 600, SDL_WINDOW_VULKAN);
    if (!window)
        throw std::runtime_error ("SDL2 window");

    slrd::APIConfig config;
    config.debug = true;
    config.app_name = "imgui test";
    config.dev_name = "slayer";
    config.engine_name = "none";
    config.api_version = { 1, 0, 0 };
    config.app_version = { 1, 0, 0 };

    std::vector<const char *> exts;
    {
        uint32_t ext_count = 0;
        SDL_Vulkan_GetInstanceExtensions (window, &ext_count, nullptr);

        exts.resize (ext_count);
        SDL_Vulkan_GetInstanceExtensions (window, &ext_count, exts.data ());
    }
    config.instance_extensions = exts;

    auto result = slrd::init (slrd::API::API_VULKAN, config);
    if (result)
        throw std::runtime_error ("Failed to initialize SLRD");

    const char *dev_exts[] = {
        "VK_KHR_swapchain"
    };

    slrd::DeviceConfig device_config;
    device_config.debug = true;
    device_config.device_extensions = dev_exts;

    device = slrd::createDevice (device_config);
    if (!device)
        throw std::runtime_error ("Failed to initialize device");

    slrd::CommandQueueInfo queue_info;
    queue_info.flags = slrd::COMMAND_QUEUE_GRAPHICS;
    queue = device->createCommandQueue (queue_info);
    if (!queue)
        throw std::runtime_error ("Failed to create the command queue");

    {
        imContext = ImGui::CreateContext ();
        if (!imContext)
            throw std::runtime_error ("Failed to create ImContext");

        if (!ImGui_ImplSDL2_InitForOther (window))
            throw std::runtime_error ("Failed to initialize ImGUI for SDL2");

        ImGui_ImplSLRD_InitInfo config;
        config.frames = NR_FIF;
        config.device = device.get ();
        config.command_queue = queue.get ();
        if (!ImGui_ImplSLRD_Init (&config))
            throw std::runtime_error ("Failed to initialize ImGUI for SLRD");
    }

    {
        slrd::SurfacePtr surface;

        slrd::SurfaceInfo surface_info;
        auto vk_data = slrd::platform::vulkan::getVulkanAPIData ();
        VkSurfaceKHR sdl_surface;
        auto result = SDL_Vulkan_CreateSurface (window, vk_data->instance, &sdl_surface);

        surface_info.apiData.ptr = sdl_surface;
        surface = slrd::createSurface (surface_info);
        if (!surface) {
            throw std::runtime_error ("Failed to create the surface");
        }

        slrd::SwapchainInfo sc_info;
        sc_info.width = 800;
        sc_info.height = 600;
        sc_info.surface = surface.get ();
        sc_info.requireVSync = true;
        sc_info.requestedImages = NR_FIF;

        swapchain = device->createSwapchain (sc_info);
        if (!swapchain)
            throw std::runtime_error ("Failed to create a swapchain");
    }

    {
        slrd::RenderPassAttachment color;
        color.format = swapchain->getFormat ();
        color.loadOp = slrd::LOAD_OPERATION_CLEAR;
        color.storeOp = slrd::STORE_OPERATION_STORE;
        color.initialLayout = slrd::TEXTURE_LAYOUT_UNDEFINED;
        color.finalLayout = slrd::TEXTURE_LAYOUT_SWAPCHAIN_SRC;
        color.presentable = true;

        slrd::RenderPassInfo info;
        info.colorAttachments = { &color, 1 };

        renderpass = device->createRenderPass (info);
        if (!renderpass)
            throw std::runtime_error ("Failed to create a renderpass");
    }

    texture = createTextureFromImage ("test.jpg", &texture_view);
    if (!texture) {
        throw std::runtime_error ("Failed to create texture from image"); 
    }
    im_texture = ImGui_ImplSLRD_AddTexture (texture.get (), texture_view.get (),
            slrd::TEXTURE_LAYOUT_SHADER_READ_ONLY);

    for (uint32_t i = 0; i < NR_FIF; ++i) {
        auto cmd_buffer = queue->getCommandBuffer ();
        if (!cmd_buffer)
            throw std::runtime_error ("Failed to create a command buffer");

        cmd_buffers[i] = cmd_buffer;
    }

    for (uint32_t i = 0; i < NR_FIF; ++i) {
        auto fence = device->createFence (true);
        if (!fence)
            throw std::runtime_error ("Failed to create a fence");

        fences[i] = std::move (fence);
    }

    bool should_recreate_swapchain = false;
    bool should_close = false;
    while (!should_close) {
        if (should_recreate_swapchain) {
            should_recreate_swapchain = false;
            recreateSwapchain ();
        }

        auto& fence = fences[current_frame];
        auto& cmd_buffer = cmd_buffers[current_frame];

        fence->wait ();
        fence->reset ();

        cmd_buffer->reset ();

        uint32_t image;
        auto res = swapchain->acquireNextImage (&image);
        if (res == slrd::SWAPCHAIN_RESULT_NEEDS_RESIZE) {
            should_recreate_swapchain = true;
            continue;
        }

        if (res != slrd::SWAPCHAIN_RESULT_SUCCESS) {
            throw std::runtime_error ("Failed to get next frame");
        }

        auto texture_view = swapchain->getTextureView (image);
        renderpass->setTextureView (0, texture_view);

        SDL_Event event;
        while (SDL_PollEvent (&event)) {
            ImGui_ImplSDL2_ProcessEvent (&event);
            switch (event.type) {
                case SDL_QUIT:
                    should_close = true;
                    break;
            }
        }

        ImGui_ImplSDL2_NewFrame ();
        ImGui_ImplSLRD_NewFrame ();

        ImGui::NewFrame (); {
            ImGui::ShowDemoWindow ();

            ImGui::Begin ("Test image");
            ImGui::Image ((ImTextureID)&im_texture, {200, 200});
            ImGui::End ();
        } ImGui::Render ();

        cmd_buffer->begin ();

        slrd::RenderPassBeginInfo begin_info;
        slrd::RenderPassColorClearValue clear_value = { 0, 0, 0, 1 };
        begin_info.colorClearValues = { &clear_value, 1 };

        cmd_buffer->beginRenderPass (renderpass.get (), begin_info);
            ImGui_ImplSLRD_RenderDrawData (ImGui::GetDrawData (), cmd_buffer);
        cmd_buffer->endRenderPass ();

        cmd_buffer->end ();

        slrd::SubmitInfo submit_info;
        submit_info.fence = fence.get ();
        submit_info.commandBuffers = { &cmd_buffer, 1 };

        queue->submit (submit_info);

        slrd::PresentInfo present_info;
        present_info.image = image;
        if (swapchain->present (present_info) == slrd::SWAPCHAIN_RESULT_NEEDS_RESIZE)
            should_recreate_swapchain = true;

        current_frame = (current_frame + 1) % NR_FIF;
    }

    device->waitIdle ();

    ImGui_ImplSLRD_RemoveTexture (im_texture);
    im_texture = nullptr;
    texture = nullptr;
    texture_view = nullptr;

    ImGui_ImplSLRD_Shutdown ();
    ImGui_ImplSDL2_Shutdown ();

    ImGui::DestroyContext (imContext);

    renderpass = nullptr;
    swapchain  = nullptr;
    queue = nullptr;

    for (auto& fence : fences)
        fence = nullptr;

    device = nullptr;

    slrd::deinit ();

    return 0;
}

void recreateSwapchain () {
    device->waitIdle ();

    int w, h;
    SDL_GL_GetDrawableSize (window, &w, &h);

    int res = swapchain->resize (w, h);
    if (res) {
        throw std::runtime_error ("Failed to resize the swapchain");
    }

    for (auto& fence : fences)
        fence = device->createFence (true);
}

slrd::TexturePtr createTextureFromImage (
        const std::filesystem::path& path, slrd::TextureViewPtr *texView) {
    if (!std::filesystem::exists (path)) {
        return nullptr;
    }

    int w, h, c;
    uint8_t *const udata = stbi_load (path.c_str (), &w, &h, &c, 4);

    auto oneTime = queue->getCommandBuffer ();
    if (!oneTime) {
        std::cout << "Failed to create one time command buffer\n";
        return nullptr;
    }

    slrd::BufferInfo bufInfo;
    bufInfo.usage = 0;
    bufInfo.gpu = true;
    bufInfo.coherent = true;
    bufInfo.properties = slrd::BUFFER_PROPERTY_TRANSFER_SRC;
    bufInfo.size = w * h * 4;
    slrd::BufferPtr stagingBuffer = device->createBuffer (bufInfo);
    if (!stagingBuffer) {
        std::cout << "Failed to create one time staging buffer\n";
        return nullptr;
    }

    void *map = stagingBuffer->map ();
    if (!map) {
        std::cout << "Failed to create a staging buffer map\n";
        return nullptr;
    }
    memcpy (map, udata, w * h * 4);
    free (udata);
    stagingBuffer->unmap ();

    slrd::TextureInfo texInfo;
    texInfo.type = slrd::TEXTURE_TYPE_2D;
    texInfo.width = w;
    texInfo.height = h;
    texInfo.usage = slrd::TEXTURE_USAGE_SAMPLED | slrd::TEXTURE_USAGE_TRANSFER_DST;
    texInfo.format = slrd::FORMAT_RGBA8_UNORM;
    texInfo.tiling = slrd::TEXTURE_TILING_OPTIMAL;

    slrd::TexturePtr texture = device->createTexture (texInfo);
    if (!texture) {
        return nullptr;
    }

    if (!texView)
        return texture;

    oneTime->begin ();

    slrd::BufferTextureCopyInfo bufTexCopyInfo;
    slrd::BufferTextureRegion region;

    region.rect = slrd::Rect3D<uint32_t>{
        0, 0, 0, (uint32_t)w, (uint32_t)h, 1
    };
    region.rows = w;
    region.height = h;
    region.textureViewInfo.arrayLayer = 0;
    region.textureViewInfo.arrayLayers = 1;
    region.textureViewInfo.mipLevel = 0;
    region.textureViewInfo.mipLevels = 1;

    bufTexCopyInfo.buffer = stagingBuffer.get ();
    bufTexCopyInfo.texture = texture.get ();
    bufTexCopyInfo.regions = { &region, 1 };

    slrd::TextureBarrierInfo tbInfo;
    tbInfo.texture = texture.get ();
    tbInfo.currentTextureLayout = slrd::TEXTURE_LAYOUT_UNDEFINED;
    tbInfo.newTextureLayout = slrd::TEXTURE_LAYOUT_TRANSFER_DST;
    tbInfo.viewInfo.mipLevels = 1;
    tbInfo.viewInfo.mipLevel  = 0;
    tbInfo.viewInfo.aspect = slrd::TEXTURE_ASPECT_COLOR;
    tbInfo.viewInfo.arrayLayer = 0;
    tbInfo.viewInfo.arrayLayers = 1;

    oneTime->pipelineTextureBarrier (tbInfo);
    oneTime->copyBufferToImage (bufTexCopyInfo);

    tbInfo.currentTextureLayout = slrd::TEXTURE_LAYOUT_TRANSFER_DST;
    tbInfo.newTextureLayout = slrd::TEXTURE_LAYOUT_SHADER_READ_ONLY;
    oneTime->pipelineTextureBarrier (tbInfo);

    oneTime->end ();

    slrd::SubmitInfo info;
    info.commandBuffers = { &oneTime, 1 };
    int res = queue->submit (info);
    if (res) {
        std::cout << "Failed to submit one time command buffer\n";
        return nullptr;
    }

    /* Wait for the operations to complete, before deleting the buffer */
    queue->wait ();

    slrd::TextureViewInfo viewInfo;
    viewInfo.mipLevels = 1;
    viewInfo.arrayLayers = 1;

    slrd::TextureViewPtr view = texture->createTextureView (viewInfo);
    if (!view) {
        return nullptr;
    }

    *texView = view;
    return texture;
}
