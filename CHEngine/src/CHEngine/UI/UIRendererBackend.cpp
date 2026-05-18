#include "chepch.h"
#include "UIRendererBackend.h"

#include "CHEngine/Application.h"
#include "CHEngine/Utils/AppPaths.h"
#include <Log/Log.h>

#include <Render/IRenderFactory.h>
#include <Render/Graph/PassDesc.h>
#include <Render/Descriptors.h>
#include <Render/Pipeline/VertexLayout.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>

namespace CHEngine {

static constexpr uint32_t kInitVerts   = 4096;
static constexpr uint32_t kInitIndices = 8192;
static constexpr uint32_t kInitDraws   = 128;

// stride = 8 floats * 4 bytes = 32 bytes
static constexpr uint32_t kUIVertexStride = 32u;

static VertexInputLayout GetUIVertexLayout()
{
    // Matches VSIn in ui_msdf.slang:
    //   float2 Position : POSITION      → attrib 0, offset  0
    //   float2 TexCoord : TEXCOORD0     → attrib 1, offset  8
    //   float2 QuadMin  : TEXCOORD1     → attrib 2, offset 16
    //   float2 QuadMax  : TEXCOORD2     → attrib 3, offset 24
    Vector<VertexAttributeDesc> attrs = {
        { VertexFormat::Float2, 0,  0, 0 },
        { VertexFormat::Float2, 0,  8, 0 },
        { VertexFormat::Float2, 0, 16, 1 },
        { VertexFormat::Float2, 0, 24, 2 },
    };
    return VertexInputLayout(attrs, kUIVertexStride);
}

// ─── dtor ────────────────────────────────────────────────────────────────────

UIRendererBackend::~UIRendererBackend()
{
    if (!m_Factory) return;
    if (m_VB.IsValid())              m_Factory->Delete(m_VB);
    if (m_IB.IsValid())              m_Factory->Delete(m_IB);
    if (m_CameraUBORing.IsValid())   m_Factory->Delete(m_CameraUBORing);
    if (m_DrawUBORing.IsValid())     m_Factory->Delete(m_DrawUBORing);
    if (m_PipelineOverlay.IsValid()) m_Factory->Delete(m_PipelineOverlay);
    if (m_PipelineWorld.IsValid())   m_Factory->Delete(m_PipelineWorld);
    if (m_Shader.IsValid())          Application::Get().Render().DestroyShader(m_Shader);
}

// ─── Init ─────────────────────────────────────────────────────────────────────

void UIRendererBackend::Init(IRenderFactory* factory)
{
    CHE_CORE_ASSERT(factory, "UIRendererBackend::Init - factory is null");
    m_Factory = factory;
}

// ─── EnsureGPUResources ──────────────────────────────────────────────────────

bool UIRendererBackend::EnsureGPUResources()
{
    if (m_Ready) return true;
    if (!m_Factory) return false;

    if (!m_Shader.IsValid())
    {
        auto path = AppPaths::ShadersDir() / "ui_msdf.slang";
        m_Shader  = Application::Get().Render().CreateShaderFromFile(
            String("UIShader"), String(path.string()));
        if (!m_Shader.IsValid())
        {
            CHE_CORE_ERROR("UIRendererBackend: failed to compile ui_msdf.slang");
            return false;
        }
    }

    auto BuildPipeline = [&](bool depthTest) -> PipelineHandle
    {
        PipelineDesc pd;
        pd.Shader       = m_Shader;
        pd.VertexLayout = GetUIVertexLayout();
        pd.Primitive    = PrimitiveType::Triangles;
        pd.Depth.Test   = depthTest;
        pd.Depth.Write  = false;
        pd.Raster.Cull  = CullMode::None;

        pd.Blend.Enable   = true;
        pd.Blend.SrcColor = BlendFactor::SrcAlpha;
        pd.Blend.DstColor = BlendFactor::OneMinusSrcAlpha;
        pd.Blend.ColorOp  = BlendOp::Add;
        pd.Blend.SrcAlpha = BlendFactor::One;
        pd.Blend.DstAlpha = BlendFactor::OneMinusSrcAlpha;
        pd.Blend.AlphaOp  = BlendOp::Add;

        pd.ColorFormats.push_back(TextureFormat::RGBA8_UNORM);
        return m_Factory->CreatePipeline(std::move(pd));
    };

    if (!m_PipelineOverlay.IsValid())
    {
        m_PipelineOverlay = BuildPipeline(false);
        if (!m_PipelineOverlay.IsValid())
        {
            CHE_CORE_ERROR("UIRendererBackend: failed to create overlay UI pipeline");
            return false;
        }
    }
    if (!m_PipelineWorld.IsValid())
    {
        m_PipelineWorld = BuildPipeline(true);
        if (!m_PipelineWorld.IsValid())
        {
            CHE_CORE_ERROR("UIRendererBackend: failed to create world UI pipeline");
            return false;
        }
    }

    // Compute aligned strides for ring buffers.
    {
        uint32_t align = m_Factory->GetUniformBufferOffsetAlignment();
        if (align == 0) align = 1;
        uint32_t dsz = sizeof(UIDrawUBOData);
        m_DrawUBOAlignedStride   = (dsz + align - 1) / align * align;
        uint32_t csz = sizeof(UICameraUBOData);
        m_CameraUBOAlignedStride = (csz + align - 1) / align * align;
    }

    EnsureVBIBCapacity(kInitVerts, kInitIndices);
    EnsureDrawUBOCapacity(kInitDraws);
    EnsureCameraUBOCapacity(8u);

    if (!m_VB.IsValid() || !m_IB.IsValid() || !m_DrawUBORing.IsValid() ||
        !m_CameraUBORing.IsValid())
    {
        CHE_CORE_ERROR("UIRendererBackend: failed to create VB/IB/UBO rings");
        return false;
    }

    m_Ready = true;
    return true;
}

void UIRendererBackend::EnsureVBIBCapacity(uint32_t neededVerts, uint32_t neededIndices)
{
    if (neededVerts > m_MaxVerts)
    {
        if (m_VB.IsValid()) m_Factory->Delete(m_VB);
        m_MaxVerts = neededVerts;
        m_VB = m_Factory->CreateBuffer(
            static_cast<uint64_t>(m_MaxVerts) * kUIVertexStride,
            BufferUsage::Vertex, MemoryType::CpuToGpu, {}, String("UI_VB"));
    }
    if (neededIndices > m_MaxIndices)
    {
        if (m_IB.IsValid()) m_Factory->Delete(m_IB);
        m_MaxIndices = neededIndices;
        m_IB = m_Factory->CreateBuffer(
            static_cast<uint64_t>(m_MaxIndices) * sizeof(uint32_t),
            BufferUsage::Index, MemoryType::CpuToGpu, {}, String("UI_IB"));
    }
}

void UIRendererBackend::EnsureDrawUBOCapacity(uint32_t neededDraws)
{
    if (neededDraws <= m_MaxDraws) return;
    if (m_DrawUBORing.IsValid()) m_Factory->Delete(m_DrawUBORing);
    m_MaxDraws = neededDraws;
    m_DrawUBORing = m_Factory->CreateBuffer(
        static_cast<uint64_t>(m_MaxDraws) * m_DrawUBOAlignedStride,
        BufferUsage::Uniform, MemoryType::CpuToGpu, {}, String("UI_DrawUBO"));
}

void UIRendererBackend::EnsureCameraUBOCapacity(uint32_t neededCams)
{
    if (neededCams <= m_MaxCameras) return;
    if (m_CameraUBORing.IsValid()) m_Factory->Delete(m_CameraUBORing);
    m_MaxCameras = neededCams;
    m_CameraUBORing = m_Factory->CreateBuffer(
        static_cast<uint64_t>(m_MaxCameras) * m_CameraUBOAlignedStride,
        BufferUsage::Uniform, MemoryType::CpuToGpu, {}, String("UI_CameraUBO"));
}

// ─── BeginFrame / BeginCanvas / EndCanvas ────────────────────────────────────

void UIRendererBackend::BeginFrame(uint32_t screenW, uint32_t screenH)
{
    m_ScreenW = screenW;
    m_ScreenH = screenH;
    m_Vertices.clear();
    m_Indices.clear();
    m_DrawUBOs.clear();
    m_CameraUBOs.clear();
    m_Batches.clear();
    m_Submissions.clear();
    m_CurrentSubmission = -1;
    m_HasClipRect = false;
}

void UIRendererBackend::BeginOverlay()
{
    glm::mat4 ortho = glm::ortho(
        0.f, static_cast<float>(m_ScreenW),
        static_cast<float>(m_ScreenH), 0.f,
        -1.f, 1.f);
    BeginCanvas(ortho, /*depthTest*/ false);
}

void UIRendererBackend::BeginCanvas(const glm::mat4& viewProj, bool depthTest)
{
    if (m_CurrentSubmission >= 0)
        EndCanvas(); // implicit close

    UICameraUBOData cam{};
    std::memcpy(cam.ViewProj, glm::value_ptr(viewProj), sizeof(cam.ViewProj));
    cam.ScreenSize[0] = static_cast<float>(m_ScreenW);
    cam.ScreenSize[1] = static_cast<float>(m_ScreenH);
    uint32_t camIdx = static_cast<uint32_t>(m_CameraUBOs.size());
    m_CameraUBOs.push_back(cam);

    Submission s{};
    s.cameraUBOIndex = camIdx;
    s.firstBatch     = static_cast<uint32_t>(m_Batches.size());
    s.batchCount     = 0;
    s.depthTest      = depthTest;
    m_Submissions.push_back(s);
    m_CurrentSubmission = static_cast<int>(m_Submissions.size()) - 1;
}

void UIRendererBackend::EndCanvas()
{
    if (m_CurrentSubmission < 0) return;
    Submission& s = m_Submissions[static_cast<size_t>(m_CurrentSubmission)];
    s.batchCount  = static_cast<uint32_t>(m_Batches.size()) - s.firstBatch;
    m_CurrentSubmission = -1;
}

void UIRendererBackend::SetClipRect(float x, float y, float w, float h)
{
    m_HasClipRect = true;
    m_ClipX = x; m_ClipY = y; m_ClipW = w; m_ClipH = h;
}

void UIRendererBackend::ClearClipRect()
{
    m_HasClipRect = false;
}

// ─── PushQuadRaw ─────────────────────────────────────────────────────────────

void UIRendererBackend::PushQuadRaw(float qx, float qy, float qw, float qh,
                                     float u0, float v0, float u1, float v1,
                                     const UIDrawUBOData& drawData,
                                     TextureHandle tex)
{
    // Original quad bounds kept for SDF (corner radius, MSDF glyph outlines).
    float origX = qx, origY = qy, origMaxX = qx + qw, origMaxY = qy + qh;

    if (m_HasClipRect && qw > 0.f && qh > 0.f)
    {
        float cx1 = std::max(qx, m_ClipX);
        float cy1 = std::max(qy, m_ClipY);
        float cx2 = std::min(qx + qw, m_ClipX + m_ClipW);
        float cy2 = std::min(qy + qh, m_ClipY + m_ClipH);
        if (cx1 >= cx2 || cy1 >= cy2) return;
        float uw = u1 - u0, vh = v1 - v0;
        float nu0 = u0 + (cx1 - qx) / qw * uw;
        float nu1 = u0 + (cx2 - qx) / qw * uw;
        float nv0 = v0 + (cy1 - qy) / qh * vh;
        float nv1 = v0 + (cy2 - qy) / qh * vh;
        qx = cx1; qy = cy1; qw = cx2 - cx1; qh = cy2 - cy1;
        u0 = nu0; u1 = nu1; v0 = nv0; v1 = nv1;
    }

    uint32_t base    = static_cast<uint32_t>(m_Vertices.size());
    float    qmaxX   = qx + qw;
    float    qmaxY   = qy + qh;

    m_Vertices.push_back({ qx,    qy,    u0, v0, origX, origY, origMaxX, origMaxY });
    m_Vertices.push_back({ qmaxX, qy,    u1, v0, origX, origY, origMaxX, origMaxY });
    m_Vertices.push_back({ qx,    qmaxY, u0, v1, origX, origY, origMaxX, origMaxY });
    m_Vertices.push_back({ qmaxX, qmaxY, u1, v1, origX, origY, origMaxX, origMaxY });

    uint32_t firstIdx = static_cast<uint32_t>(m_Indices.size());
    m_Indices.push_back(base + 0); m_Indices.push_back(base + 1); m_Indices.push_back(base + 3);
    m_Indices.push_back(base + 0); m_Indices.push_back(base + 3); m_Indices.push_back(base + 2);

    uint32_t drawIdx = static_cast<uint32_t>(m_DrawUBOs.size());
    m_DrawUBOs.push_back(drawData);

    m_Batches.push_back({ firstIdx, 6u, drawIdx, tex });
}

// ─── DrawQuad ────────────────────────────────────────────────────────────────

void UIRendererBackend::DrawQuad(float x, float y, float w, float h,
                                  glm::vec4 color,
                                  float cornerRadius, float borderWidth,
                                  glm::vec4 borderColor)
{
    UIDrawUBOData d{};
    d.ColorA[0] = color.r;       d.ColorA[1] = color.g;
    d.ColorA[2] = color.b;       d.ColorA[3] = color.a;
    d.ColorB[0] = borderColor.r; d.ColorB[1] = borderColor.g;
    d.ColorB[2] = borderColor.b; d.ColorB[3] = borderColor.a;
    d.Params[0] = cornerRadius;
    d.Params[1] = borderWidth;
    d.Mode = 0u;
    PushQuadRaw(x, y, w, h, 0.f, 0.f, 1.f, 1.f, d, TextureHandle{});
}

// ─── DrawTexturedQuad ────────────────────────────────────────────────────────

void UIRendererBackend::DrawTexturedQuad(float x, float y, float w, float h,
                                          TextureHandle tex, glm::vec4 tint,
                                          float cornerRadius)
{
    UIDrawUBOData d{};
    d.ColorA[0] = tint.r; d.ColorA[1] = tint.g;
    d.ColorA[2] = tint.b; d.ColorA[3] = tint.a;
    d.Params[0] = cornerRadius;
    d.Mode = 1u;
    PushQuadRaw(x, y, w, h, 0.f, 0.f, 1.f, 1.f, d, tex);
}

// ─── DrawUIText ────────────────────────────────────────────────────────────────

void UIRendererBackend::DrawUIText(const std::string& text,
                                  float x, float y, float /*maxW*/, float /*maxH*/,
                                  FontAtlasHandle fontHandle, float fontSize,
                                  glm::vec4 color)
{
    if (text.empty() || !fontHandle.IsValid()) return;

    const FontAtlas* atlas = m_AtlasLoader.Get(fontHandle);
    if (!atlas) return;

    float emSize = atlas->GetEmSize();
    float scale  = (emSize > 0.f) ? (fontSize / emSize) : fontSize;
    float dr     = atlas->GetDistanceRange();

    UIDrawUBOData drawData{};
    drawData.ColorA[0] = color.r; drawData.ColorA[1] = color.g;
    drawData.ColorA[2] = color.b; drawData.ColorA[3] = color.a;
    drawData.Params[2] = dr;
    drawData.Mode = 2u;

    uint32_t drawIdx  = static_cast<uint32_t>(m_DrawUBOs.size());
    m_DrawUBOs.push_back(drawData);

    uint32_t firstIdx = static_cast<uint32_t>(m_Indices.size());
    uint32_t idxCount = 0;

    float cursorX   = x;
    float baselineY = y + atlas->GetAscender() * scale;

    for (size_t i = 0; i < text.size(); )
    {
        uint32_t      cp;
        unsigned char c = static_cast<unsigned char>(text[i]);

        if (c < 0x80u)
        {
            cp = c; ++i;
        }
        else if ((c & 0xE0u) == 0xC0u && i + 1 < text.size())
        {
            cp = static_cast<uint32_t>(c & 0x1Fu) << 6;
            cp |= static_cast<unsigned char>(text[i + 1]) & 0x3Fu;
            i += 2;
        }
        else if ((c & 0xF0u) == 0xE0u && i + 2 < text.size())
        {
            cp  = static_cast<uint32_t>(c & 0x0Fu) << 12;
            cp |= (static_cast<unsigned char>(text[i + 1]) & 0x3Fu) << 6;
            cp |= static_cast<unsigned char>(text[i + 2]) & 0x3Fu;
            i += 3;
        }
        else { ++i; continue; }

        if (cp == '\n')
        {
            cursorX   = x;
            baselineY += atlas->GetLineHeight() * scale;
            continue;
        }

        const GlyphMetrics* gm = atlas->GetGlyph(cp);
        if (!gm)
        {
            gm = atlas->GetGlyph(0x20u); // space fallback
            if (!gm) { cursorX += fontSize * 0.3f; continue; }
        }

        bool hasAtlas = (gm->atlasRight  > gm->atlasLeft &&
                         gm->atlasTop    > gm->atlasBottom);
        if (hasAtlas)
        {
            float sl = cursorX + gm->planeLeft   * scale;
            float sr = cursorX + gm->planeRight  * scale;
            float st = baselineY - gm->planeTop  * scale;  // screen Y: up = smaller
            float sb = baselineY - gm->planeBottom * scale;

            // atlasTop > atlasBottom in the JSON (pixel coords, Y=0 bottom in msdfgen).
            // After our Y-flip during PNG save, atlasTop → low UV, atlasBottom → high UV.
            // But OpenGL also loads the PNG bottom-up, so both flips cancel:
            //   atlasTop (high msdfgen Y) → high OpenGL UV → screen top ✓
            float u0 = gm->atlasLeft,  u1 = gm->atlasRight;
            float v0 = gm->atlasTop,   v1 = gm->atlasBottom;

            // Save original glyph bounds for MSDF SDF (used as qmin/qmax in vertex).
            float origSl = sl, origSt = st, origSr = sr, origSb = sb;

            if (m_HasClipRect)
            {
                float cx1 = std::max(sl, m_ClipX),           cy1 = std::max(st, m_ClipY);
                float cx2 = std::min(sr, m_ClipX + m_ClipW), cy2 = std::min(sb, m_ClipY + m_ClipH);
                if (cx1 >= cx2 || cy1 >= cy2) { cursorX += gm->advance * scale; continue; }
                float gw = sr - sl, gh = sb - st;
                float nu0 = u0 + (cx1 - sl) / gw * (u1 - u0);
                float nu1 = u0 + (cx2 - sl) / gw * (u1 - u0);
                float nv0 = v0 + (cy1 - st) / gh * (v1 - v0);
                float nv1 = v0 + (cy2 - st) / gh * (v1 - v0);
                sl = cx1; sr = cx2; st = cy1; sb = cy2;
                u0 = nu0; u1 = nu1; v0 = nv0; v1 = nv1;
            }

            uint32_t base = static_cast<uint32_t>(m_Vertices.size());
            m_Vertices.push_back({ sl, st, u0, v0, origSl, origSt, origSr, origSb });
            m_Vertices.push_back({ sr, st, u1, v0, origSl, origSt, origSr, origSb });
            m_Vertices.push_back({ sl, sb, u0, v1, origSl, origSt, origSr, origSb });
            m_Vertices.push_back({ sr, sb, u1, v1, origSl, origSt, origSr, origSb });

            m_Indices.push_back(base + 0); m_Indices.push_back(base + 1); m_Indices.push_back(base + 3);
            m_Indices.push_back(base + 0); m_Indices.push_back(base + 3); m_Indices.push_back(base + 2);
            idxCount += 6;
        }

        cursorX += gm->advance * scale;
    }

    if (idxCount > 0)
        m_Batches.push_back({ firstIdx, idxCount, drawIdx, atlas->GetTexture() });
}

// ─── Flush ───────────────────────────────────────────────────────────────────

void UIRendererBackend::Flush()
{
    if (m_CurrentSubmission >= 0)
        EndCanvas(); // safety: close dangling submission

    if (m_Batches.empty() || m_Submissions.empty()) return;
    if (!EnsureGPUResources()) return;

    TextureHandle outputTex = Application::Get().Render().GetViewportOutputTexture();
    if (!outputTex.IsValid()) return;
    TextureHandle depthTex = Application::Get().Render().GetViewportDepthTexture();

    uint32_t totalVerts   = static_cast<uint32_t>(m_Vertices.size());
    uint32_t totalIndices = static_cast<uint32_t>(m_Indices.size());
    uint32_t totalDraws   = static_cast<uint32_t>(m_DrawUBOs.size());
    uint32_t totalCams    = static_cast<uint32_t>(m_CameraUBOs.size());

    EnsureVBIBCapacity(totalVerts, totalIndices);
    EnsureDrawUBOCapacity(totalDraws);
    EnsureCameraUBOCapacity(totalCams);

    // Upload VB
    m_Factory->UpdateBuffer(m_VB,
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(m_Vertices.data()),
            totalVerts * kUIVertexStride));

    // Upload IB
    m_Factory->UpdateBuffer(m_IB,
        std::span<const std::byte>(
            reinterpret_cast<const std::byte*>(m_Indices.data()),
            totalIndices * sizeof(uint32_t)));

    // Upload camera UBO ring
    {
        std::vector<std::byte> ring(
            static_cast<size_t>(totalCams) * m_CameraUBOAlignedStride, std::byte{0});
        for (uint32_t i = 0; i < totalCams; ++i)
        {
            std::memcpy(
                ring.data() + static_cast<size_t>(i) * m_CameraUBOAlignedStride,
                &m_CameraUBOs[i], sizeof(UICameraUBOData));
        }
        m_Factory->UpdateBuffer(m_CameraUBORing,
            std::span<const std::byte>(ring.data(), ring.size()));
    }

    // Upload draw UBO ring
    {
        std::vector<std::byte> ring(
            static_cast<size_t>(totalDraws) * m_DrawUBOAlignedStride, std::byte{0});
        for (uint32_t i = 0; i < totalDraws; ++i)
        {
            std::memcpy(
                ring.data() + static_cast<size_t>(i) * m_DrawUBOAlignedStride,
                &m_DrawUBOs[i], sizeof(UIDrawUBOData));
        }
        m_Factory->UpdateBuffer(m_DrawUBORing,
            std::span<const std::byte>(ring.data(), ring.size()));
    }

    // One pass per submission.
    for (const Submission& sub : m_Submissions)
    {
        if (sub.batchCount == 0) continue;

        PassDesc pass;
        pass.Name            = sub.depthTest ? "UIPass_World" : "UIPass_Overlay";
        pass.Pipeline        = sub.depthTest ? m_PipelineWorld : m_PipelineOverlay;
        pass.ColorAttachments.push_back(outputTex);
        pass.ColorLoadOp     = ELoadOp::Load;
        pass.ColorStoreOp    = EStoreOp::Store;
        pass.ViewportWidth   = m_ScreenW;
        pass.ViewportHeight  = m_ScreenH;
        pass.Reads.push_back(outputTex);
        pass.Writes.push_back(outputTex);

        if (sub.depthTest && depthTex.IsValid())
        {
            pass.DepthAttachment = depthTex;
            pass.DepthLoadOp     = ELoadOp::Load;
            pass.Reads.push_back(depthTex);
        }
        else
        {
            pass.DepthLoadOp = ELoadOp::DontCare;
        }

        // Per-pass camera UBO at slot 0
        pass.Uniforms.push_back({
            m_CameraUBORing, 0u,
            static_cast<uint64_t>(sub.cameraUBOIndex) * m_CameraUBOAlignedStride,
            sizeof(UICameraUBOData)
        });

        for (uint32_t bi = 0; bi < sub.batchCount; ++bi)
        {
            const BatchEntry& batch = m_Batches[sub.firstBatch + bi];

            DrawDesc draw;
            draw.VertexBuffer  = m_VB;
            draw.IndexBuffer   = m_IB;
            draw.IdxFormat     = IndexFormat::UInt32;
            draw.IndexCount    = batch.indexCount;
            draw.FirstIndex    = batch.firstIndex;
            draw.BaseVertex    = 0;
            draw.InstanceCount = 1;

            draw.Uniforms.push_back({
                m_DrawUBORing, 1u,
                static_cast<uint64_t>(batch.drawUBOIndex) * m_DrawUBOAlignedStride,
                sizeof(UIDrawUBOData)
            });

            if (batch.texture.IsValid())
                draw.Textures.push_back({ batch.texture, 0u });

            pass.Draws.push_back(std::move(draw));
        }

        Application::Get().Render().GetFrameGraph().AddPass(std::move(pass));
    }
}

} // namespace CHEngine
