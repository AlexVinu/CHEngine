#include "FontAtlas.h"
#include "CHEngine/Application.h"

namespace CHEngine {

FontAtlas::FontAtlas(TextureHandle texture,
                     std::unordered_map<uint32_t, GlyphMetrics> glyphs,
                     float distanceRange,
                     float atlasWidth,
                     float atlasHeight,
                     float emSize,
                     float lineHeight,
                     float ascender,
                     float descender)
    : m_Texture(texture)
    , m_Glyphs(std::move(glyphs))
    , m_DistanceRange(distanceRange)
    , m_AtlasWidth(atlasWidth)
    , m_AtlasHeight(atlasHeight)
    , m_EmSize(emSize)
    , m_LineHeight(lineHeight)
    , m_Ascender(ascender)
    , m_Descender(descender)
{}

FontAtlas::~FontAtlas()
{
    if (m_Texture.IsValid())
        Application::Get().Render().DestroyTexture(m_Texture);
}

const GlyphMetrics* FontAtlas::GetGlyph(uint32_t codepoint) const
{
    auto it = m_Glyphs.find(codepoint);
    return (it != m_Glyphs.end()) ? &it->second : nullptr;
}

} // namespace CHEngine
