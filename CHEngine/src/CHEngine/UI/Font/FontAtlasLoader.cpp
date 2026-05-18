#include "FontAtlasLoader.h"

#include "CHEngine/Application.h"
#include <Log/Log.h>

#include <nlohmann/json.hpp>
#include <fstream>

#ifdef CHE_UI_NATIVE
#include <msdf-atlas-gen/msdf-atlas-gen.h>

// Declarations only — STB_IMAGE_WRITE_IMPLEMENTATION is provided by ModelLoader_GLTF.cpp
#include <tinygltf/stb_image_write.h>
#endif

namespace CHEngine {

// ─── helpers ─────────────────────────────────────────────────────────────────

static std::filesystem::path PngPath(const std::filesystem::path& ttf)
{
    return ttf.parent_path() / (ttf.stem().string() + ".png");
}

static std::filesystem::path JsonPath(const std::filesystem::path& ttf)
{
    return ttf.parent_path() / (ttf.stem().string() + ".json");
}

static bool ParseAtlasJson(const std::filesystem::path& jsonPath,
                           std::unordered_map<uint32_t, GlyphMetrics>& outGlyphs,
                           float& outDistanceRange,
                           float& outAtlasW, float& outAtlasH,
                           float& outEmSize, float& outLineHeight,
                           float& outAscender, float& outDescender)
{
    std::ifstream f(jsonPath);
    if (!f.is_open()) {
        CHE_CORE_ERROR("FontAtlasLoader: cannot open {}", jsonPath.string());
        return false;
    }

    nlohmann::json j;
    try { f >> j; }
    catch (const std::exception& e) {
        CHE_CORE_ERROR("FontAtlasLoader: JSON parse error in {}: {}", jsonPath.string(), e.what());
        return false;
    }

    outDistanceRange = j["atlas"]["distanceRange"].get<float>();
    outAtlasW        = j["atlas"]["width"].get<float>();
    outAtlasH        = j["atlas"]["height"].get<float>();
    outEmSize        = j["metrics"]["emSize"].get<float>();
    outLineHeight    = j["metrics"]["lineHeight"].get<float>();
    outAscender      = j["metrics"]["ascender"].get<float>();
    outDescender     = j["metrics"]["descender"].get<float>();

    for (const auto& g : j["glyphs"])
    {
        uint32_t cp = g["unicode"].get<uint32_t>();
        GlyphMetrics m;
        m.advance = g["advance"].get<float>();

        if (g.contains("planeBounds") && !g["planeBounds"].is_null()) {
            m.planeLeft   = g["planeBounds"]["left"].get<float>();
            m.planeBottom = g["planeBounds"]["bottom"].get<float>();
            m.planeRight  = g["planeBounds"]["right"].get<float>();
            m.planeTop    = g["planeBounds"]["top"].get<float>();
        }
        if (g.contains("atlasBounds") && !g["atlasBounds"].is_null()) {
            m.atlasLeft   = g["atlasBounds"]["left"].get<float>()   / outAtlasW;
            m.atlasBottom = g["atlasBounds"]["bottom"].get<float>() / outAtlasH;
            m.atlasRight  = g["atlasBounds"]["right"].get<float>()  / outAtlasW;
            m.atlasTop    = g["atlasBounds"]["top"].get<float>()    / outAtlasH;
        }

        outGlyphs[cp] = m;
    }

    return true;
}

// ─── public API ──────────────────────────────────────────────────────────────

FontAtlasHandle FontAtlasLoader::Load(const std::filesystem::path& ttfPath, float fontSize)
{
    CHE_CORE_ASSERT(ttfPath.is_absolute(), "FontAtlasLoader::Load - path must be absolute");

    auto key = ttfPath.string();
    if (auto it = m_Cache.find(key); it != m_Cache.end())
        return it->second;

    Scope<FontAtlas> atlas = LoadFromCache(ttfPath);

#ifdef CHE_UI_NATIVE
    if (!atlas)
        atlas = GenerateAndCache(ttfPath, fontSize);
#endif

    if (!atlas) {
        CHE_CORE_ERROR("FontAtlasLoader: failed to load atlas for '{}'", key);
        return FontAtlasHandle{};
    }

    FontAtlasHandle handle = m_Pool.Add(atlas.release());
    m_Cache[key] = handle;
    return handle;
}

void FontAtlasLoader::Unload(FontAtlasHandle handle)
{
    for (auto it = m_Cache.begin(); it != m_Cache.end(); ++it) {
        if (it->second == handle) { m_Cache.erase(it); break; }
    }
    m_Pool.Remove(handle);
}

// ─── offline load ─────────────────────────────────────────────────────────────

Scope<FontAtlas> FontAtlasLoader::LoadFromCache(const std::filesystem::path& ttfPath)
{
    auto pngPath  = PngPath(ttfPath);
    auto jsonPath = JsonPath(ttfPath);

    if (!std::filesystem::exists(pngPath) || !std::filesystem::exists(jsonPath))
        return nullptr;

    std::unordered_map<uint32_t, GlyphMetrics> glyphs;
    float dr=4.f, aw=512.f, ah=512.f, em=1.f, lh=1.2f, asc=0.8f, desc=-0.2f;

    if (!ParseAtlasJson(jsonPath, glyphs, dr, aw, ah, em, lh, asc, desc))
        return nullptr;

    TextureHandle tex = Application::Get().Render().CreateTextureFromFile(pngPath.string());
    if (!tex.IsValid()) {
        CHE_CORE_ERROR("FontAtlasLoader: failed to upload atlas texture '{}'", pngPath.string());
        return nullptr;
    }

    CHE_CORE_INFO("FontAtlasLoader: loaded atlas '{}' ({} glyphs)",
                  ttfPath.filename().string(), glyphs.size());
    return MakeScope<FontAtlas>(tex, std::move(glyphs), dr, aw, ah, em, lh, asc, desc);
}

// ─── runtime generation ──────────────────────────────────────────────────────

#ifdef CHE_UI_NATIVE

Scope<FontAtlas> FontAtlasLoader::GenerateAndCache(const std::filesystem::path& ttfPath, float fontSize)
{
    using namespace msdf_atlas;

    CHE_CORE_INFO("FontAtlasLoader: generating MSDF atlas for '{}'...",
                  ttfPath.filename().string());

    msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
    if (!ft) {
        CHE_CORE_ERROR("FontAtlasLoader: failed to initialize FreeType");
        return nullptr;
    }

    msdfgen::FontHandle* font = msdfgen::loadFont(ft, ttfPath.string().c_str());
    if (!font) {
        CHE_CORE_ERROR("FontAtlasLoader: failed to load font '{}'", ttfPath.string());
        msdfgen::deinitializeFreetype(ft);
        return nullptr;
    }

    std::vector<GlyphGeometry> glyphGeometries;
    FontGeometry fontGeometry(&glyphGeometries);

    // ASCII 0x20-0x7E
    fontGeometry.loadCharset(font, 1.0, Charset::ASCII);

    // Basic Cyrillic 0x0400-0x04FF
    {
        Charset cyrillic;
        for (uint32_t cp = 0x0400; cp <= 0x04FF; ++cp)
            cyrillic.add(static_cast<unicode_t>(cp));
        fontGeometry.loadCharset(font, 1.0, cyrillic);
    }

    TightAtlasPacker packer;
    packer.setDimensionsConstraint(DimensionsConstraint::POWER_OF_TWO_SQUARE);
    packer.setScale(fontSize);
    packer.setPixelRange(4.0);
    packer.setMiterLimit(1.0);
    packer.pack(glyphGeometries.data(), static_cast<int>(glyphGeometries.size()));

    int atlasW = 0, atlasH = 0;
    packer.getDimensions(atlasW, atlasH);

    // MTSDF: 4 channels (multi-channel SDF + true SDF)
    ImmediateAtlasGenerator<float, 4, mtsdfGenerator,
        BitmapAtlasStorage<float, 4>> generator(atlasW, atlasH);

    GeneratorAttributes attrs;
    attrs.config.overlapSupport = true;
    attrs.scanlinePass = true;
    generator.setAttributes(attrs);
    generator.setThreadCount(1);
    generator.generate(glyphGeometries.data(), static_cast<int>(glyphGeometries.size()));

    // Extract pixels: BitmapAtlasStorage → BitmapConstRef
    msdfgen::BitmapConstRef<float, 4> bitmapRef =
        static_cast<msdfgen::BitmapConstRef<float, 4>>(generator.atlasStorage());

    std::vector<uint8_t> pixels(atlasW * atlasH * 4);
    for (int y = 0; y < atlasH; ++y) {
        for (int x = 0; x < atlasW; ++x) {
            // msdfgen stores bottom-to-top; flip for PNG (top-to-bottom)
            const float* src = bitmapRef(x, atlasH - 1 - y);
            uint8_t* dst = pixels.data() + (y * atlasW + x) * 4;
            for (int c = 0; c < 4; ++c)
                dst[c] = static_cast<uint8_t>(
                    std::clamp(src[c] * 255.f + 0.5f, 0.f, 255.f));
        }
    }

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);

    // Save PNG
    auto pngPath  = PngPath(ttfPath);
    auto jsonPath = JsonPath(ttfPath);
    std::filesystem::create_directories(pngPath.parent_path());

    if (!stbi_write_png(pngPath.string().c_str(), atlasW, atlasH, 4,
                        pixels.data(), atlasW * 4)) {
        CHE_CORE_ERROR("FontAtlasLoader: failed to write '{}'", pngPath.string());
        return nullptr;
    }

    // Build and save JSON
    const msdfgen::FontMetrics& fm = fontGeometry.getMetrics();
    nlohmann::json j;
    j["atlas"]["type"]          = "mtsdf";
    j["atlas"]["distanceRange"] = 4;
    j["atlas"]["size"]          = fontSize;
    j["atlas"]["width"]         = atlasW;
    j["atlas"]["height"]        = atlasH;
    j["metrics"]["emSize"]             = fm.emSize;
    j["metrics"]["lineHeight"]         = fm.lineHeight;
    j["metrics"]["ascender"]           = fm.ascenderY;
    j["metrics"]["descender"]          = fm.descenderY;
    j["metrics"]["underlineY"]         = fm.underlineY;
    j["metrics"]["underlineThickness"] = fm.underlineThickness;

    nlohmann::json glyphsJson = nlohmann::json::array();
    for (const auto& gg : glyphGeometries) {
        nlohmann::json gj;
        gj["unicode"] = gg.getCodepoint();
        gj["advance"] = gg.getAdvance();

        if (gg.isWhitespace()) {
            gj["planeBounds"] = nullptr;
            gj["atlasBounds"] = nullptr;
        } else {
            double pl, pb, pr, pt, al, ab, ar, at;
            gg.getQuadPlaneBounds(pl, pb, pr, pt);
            gg.getQuadAtlasBounds(al, ab, ar, at);
            gj["planeBounds"] = {{"left",pl},{"bottom",pb},{"right",pr},{"top",pt}};
            gj["atlasBounds"] = {{"left",al},{"bottom",ab},{"right",ar},{"top",at}};
        }
        glyphsJson.push_back(gj);
    }
    j["glyphs"] = glyphsJson;

    { std::ofstream jf(jsonPath); jf << j.dump(2); }

    CHE_CORE_INFO("FontAtlasLoader: generated atlas {}x{}, saved to '{}'",
                  atlasW, atlasH, pngPath.string());

    return LoadFromCache(ttfPath);
}

#endif // CHE_UI_NATIVE

} // namespace CHEngine
