#pragma once

#include "FontAtlas.h"
#include "Memory/Handle.h"
#include "Memory/HandlePool.h"

#include <filesystem>
#include <unordered_map>
#include <memory>

namespace CHEngine {

struct FontAtlasTag {};
using FontAtlasHandle = Handle<FontAtlasTag>;

// Loads (or generates) MSDF font atlases and caches them by TTF path.
//
// Offline path (preferred):
//   Looks for <font>.png + <font>.json next to the .ttf file.
//   If found, loads the texture and parses the JSON metrics.
//
// Runtime fallback (CHE_UI_NATIVE):
//   If the cached files are absent, uses msdf-atlas-gen to generate
//   them in-memory, saves to disk, then proceeds as offline.
class CHENGINE_API FontAtlasLoader
{
public:
    FontAtlasLoader()  = default;
    ~FontAtlasLoader() = default;

    FontAtlasLoader(const FontAtlasLoader&)            = delete;
    FontAtlasLoader& operator=(const FontAtlasLoader&) = delete;

    // Load atlas for the given TTF/OTF path.
    // Caches by absolute path; returns the same handle on repeated calls.
    FontAtlasHandle  Load(const std::filesystem::path& ttfPath, float fontSize = 32.f);
    void             Unload(FontAtlasHandle handle);

    const FontAtlas* Get(FontAtlasHandle h) const { return m_Pool.Get(h); }
    FontAtlas*       GetMut(FontAtlasHandle h)    { return m_Pool.Get(h); }

private:
    Scope<FontAtlas> LoadFromCache(const std::filesystem::path& ttfPath);

#ifdef CHE_UI_NATIVE
    Scope<FontAtlas> GenerateAndCache(const std::filesystem::path& ttfPath, float fontSize);
#endif

    HandlePool<FontAtlas, FontAtlasTag>              m_Pool{ [](FontAtlas* p) { delete p; } };
    std::unordered_map<std::string, FontAtlasHandle> m_Cache; // absolute path → handle
};

} // namespace CHEngine
