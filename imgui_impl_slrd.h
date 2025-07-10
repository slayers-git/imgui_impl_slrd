#ifndef __IMGUI_IMPL_SLRD_H__
#define __IMGUI_IMPL_SLRD_H__

#include "slrd/format.hpp"
#include "slrd/types.hpp"
#include "imgui.h"      // IMGUI_IMPL_API

struct ImGui_ImplSLRD_InitInfo {
    uint32_t frames;

    slrd::DevicePtr device;
    slrd::CommandQueuePtr command_queue;
};

/**
 * @brief Initialize ImGUI for use with SLRD.
 *
 * @note SLRD must already be initialized by the point this function is called */
IMGUI_IMPL_API bool             ImGui_ImplSLRD_Init (ImGui_ImplSLRD_InitInfo* info);
/**
 * @brief Destroy the ImGUI backend state */
IMGUI_IMPL_API void             ImGui_ImplSLRD_Shutdown ();
IMGUI_IMPL_API void             ImGui_ImplSLRD_NewFrame ();
IMGUI_IMPL_API void             ImGui_ImplSLRD_RenderDrawData (ImDrawData* draw_data, slrd::CommandBufferPtr& command_buffer);

IMGUI_IMPL_API slrd::UniformSetPtr ImGui_ImplSLRD_AddTexture (slrd::TexturePtr texture,
        slrd::TextureViewPtr view, slrd::TextureLayout layout);
IMGUI_IMPL_API void                ImGui_ImplSLRD_RemoveTexture (slrd::UniformSetPtr);

#endif /* #define __IMGUI_IMPL_SLRD_H__ */
