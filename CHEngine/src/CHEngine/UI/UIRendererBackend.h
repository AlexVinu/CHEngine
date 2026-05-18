#pragma once

#include "Core.h"
#include "Render/Handles.h"
#include "Font/FontAtlasLoader.h"

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace CHEngine {

struct IRenderFactory;

// Batched quad renderer for in-game UI.
// Collects quads, text glyphs, and textured rects each frame, then emits a
// single FrameGraph pass (UIPass) that composites onto the LDR viewport output.
class CHENGINE_API UIRendererBackend
{
public:
    UIRendererBackend()  = default;
    ~UIRendererBackend();

    UIRendererBackend(const UIRendererBackend&)            = delete;
    UIRendererBackend& operator=(const UIRendererBackend&) = delete;

    void Init(IRenderFactory* factory);

    // Call once per frame before any BeginCanvas/Draw* calls.
    void BeginFrame(uint32_t screenW, uint32_t screenH);

    // Convenience: opens a screen-space overlay submission (ortho 0..W / H..0, no depth).
    void BeginOverlay();

    // Generic: opens a submission with custom viewProj (maps "canvas pixel" coords
    // passed to DrawQuad/Text directly to clip space). depthTest=true makes the
    // submission interact with scene depth (world canvases).
    void BeginCanvas(const glm::mat4& viewProj, bool depthTest);

    // Closes the current submission. All subsequent Draw* calls require another Begin*.
    void EndCanvas();

    // Mode 0: solid color quad with optional rounded corners + border.
    void DrawQuad(float x, float y, float w, float h,
                  glm::vec4 color,
                  float cornerRadius  = 0.f,
                  float borderWidth   = 0.f,
                  glm::vec4 borderColor = glm::vec4(0.f));

    // Mode 1: textured quad (UIImage) with tint and optional rounded clip.
    void DrawTexturedQuad(float x, float y, float w, float h,
                          TextureHandle tex,
                          glm::vec4 tint       = glm::vec4(1.f),
                          float cornerRadius   = 0.f);

    // Mode 2: MTSDF text.
    // fontHandle must be valid; fontSize is target height in screen pixels.
    void DrawUIText(const std::string& text,
                  float x, float y, float maxW, float maxH,
                  FontAtlasHandle fontHandle, float fontSize,
                  glm::vec4 color);

    // Software clip rect — DrawQuad and DrawUIText skip/clamp geometry outside this rect.
    // Set before drawing canvas children, clear (or set to full screen) after.
    void SetClipRect(float x, float y, float w, float h);
    void ClearClipRect();

    // Submit accumulated draws to the FrameGraph.
    void Flush();

    FontAtlasLoader& GetAtlasLoader() { return m_AtlasLoader; }
    bool             IsReady()  const { return m_Ready; }

private:
    struct UIVertex {
        float x, y;          // screen-space position (pixels)
        float u, v;          // UV
        float qminX, qminY;  // quad top-left  (pixels)
        float qmaxX, qmaxY;  // quad bot-right (pixels)
    };

    struct alignas(16) UICameraUBOData {
        float ViewProj[16];   // column-major mat4 (overlay = ortho, world = cameraVP*model*pixelToLocal)
        float ScreenSize[2];
        float _pad0, _pad1;
    };

    struct alignas(16) UIDrawUBOData {
        float    ColorA[4];
        float    ColorB[4];
        float    Params[4];   // [cornerRadius, borderWidth, distanceRange, 0]
        uint32_t Mode;        // 0=solid, 1=textured, 2=MSDF
        uint32_t _pad0, _pad1, _pad2;
    };

    struct BatchEntry {
        uint32_t      firstIndex;
        uint32_t      indexCount;
        uint32_t      drawUBOIndex;
        TextureHandle texture;
    };

    struct Submission {
        uint32_t cameraUBOIndex;
        uint32_t firstBatch;
        uint32_t batchCount;
        bool     depthTest;
    };

    bool EnsureGPUResources();
    void EnsureVBIBCapacity(uint32_t neededVerts, uint32_t neededIndices);
    void EnsureDrawUBOCapacity(uint32_t neededDraws);
    void EnsureCameraUBOCapacity(uint32_t neededCams);

    void PushQuadRaw(float qx, float qy, float qw, float qh,
                     float u0, float v0, float u1, float v1,
                     const UIDrawUBOData& drawData,
                     TextureHandle tex);

    IRenderFactory* m_Factory = nullptr;
    FontAtlasLoader m_AtlasLoader;

    // CPU-side per-frame accumulators
    std::vector<UIVertex>        m_Vertices;
    std::vector<uint32_t>        m_Indices;
    std::vector<UIDrawUBOData>   m_DrawUBOs;
    std::vector<UICameraUBOData> m_CameraUBOs;
    std::vector<BatchEntry>      m_Batches;
    std::vector<Submission>      m_Submissions;
    int                          m_CurrentSubmission = -1;

    // GPU resources
    ShaderHandle   m_Shader;
    PipelineHandle m_PipelineOverlay; // depth off
    PipelineHandle m_PipelineWorld;   // depth test on, write off
    BufferHandle   m_VB;
    BufferHandle   m_IB;
    BufferHandle   m_CameraUBORing;
    BufferHandle   m_DrawUBORing;

    uint32_t m_MaxVerts              = 0;
    uint32_t m_MaxIndices            = 0;
    uint32_t m_MaxDraws              = 0;
    uint32_t m_MaxCameras            = 0;
    uint32_t m_DrawUBOAlignedStride  = 0;
    uint32_t m_CameraUBOAlignedStride= 0;

    uint32_t m_ScreenW = 0;
    uint32_t m_ScreenH = 0;
    bool     m_Ready   = false;

    bool  m_HasClipRect = false;
    float m_ClipX = 0.f, m_ClipY = 0.f, m_ClipW = 0.f, m_ClipH = 0.f;
};

} // namespace CHEngine
