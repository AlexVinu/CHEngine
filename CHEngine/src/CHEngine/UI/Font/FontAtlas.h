#pragma once

#include "Core.h"
#include "Render/Handles.h"

#include <unordered_map>
#include <string>
#include <filesystem>

namespace CHEngine {

struct GlyphMetrics
{
    // UV coords in atlas texture (normalized 0..1)
    float atlasLeft   = 0.f;
    float atlasBottom = 0.f;
    float atlasRight  = 0.f;
    float atlasTop    = 0.f;

    // Glyph quad bounds in em units (relative to cursor baseline)
    float planeLeft   = 0.f;
    float planeBottom = 0.f;
    float planeRight  = 0.f;
    float planeTop    = 0.f;

    // Horizontal advance in em units
    float advance = 0.f;
};

// RAII owner of a MSDF/MTSDF font atlas GPU texture + per-glyph metadata.
// Created by FontAtlasLoader; destroyed when the loader drops the cache entry.
class CHENGINE_API FontAtlas
{
public:
    FontAtlas(TextureHandle texture,
              std::unordered_map<uint32_t, GlyphMetrics> glyphs,
              float distanceRange,
              float atlasWidth,
              float atlasHeight,
              float emSize,
              float lineHeight,
              float ascender,
              float descender);
    ~FontAtlas();

    FontAtlas(const FontAtlas&)            = delete;
    FontAtlas& operator=(const FontAtlas&) = delete;

    TextureHandle GetTexture()      const { return m_Texture; }
    float         GetDistanceRange() const { return m_DistanceRange; }
    float         GetAtlasWidth()   const { return m_AtlasWidth; }
    float         GetAtlasHeight()  const { return m_AtlasHeight; }
    float         GetEmSize()       const { return m_EmSize; }
    float         GetLineHeight()   const { return m_LineHeight; }
    float         GetAscender()     const { return m_Ascender; }
    float         GetDescender()    const { return m_Descender; }

    // Returns nullptr if glyph not in atlas.
    const GlyphMetrics* GetGlyph(uint32_t codepoint) const;

private:
    TextureHandle                           m_Texture;
    std::unordered_map<uint32_t, GlyphMetrics> m_Glyphs;
    float m_DistanceRange = 4.f;
    float m_AtlasWidth    = 512.f;
    float m_AtlasHeight   = 512.f;
    float m_EmSize        = 1.f;
    float m_LineHeight    = 1.2f;
    float m_Ascender      = 0.8f;
    float m_Descender     = -0.2f;
};

} // namespace CHEngine
