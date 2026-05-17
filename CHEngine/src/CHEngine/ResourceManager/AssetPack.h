#pragma once

// AssetPack — engine-level interface for .chepak asset archives.
// PakReader and PakBuilder are the implementation (defined in Core/FileSystem/PakFile.h).
// Exposed here so engine users #include <CHEngine/ResourceManager/AssetPack.h>
// rather than reaching into Core internals.
//
// Future: AssetPackLoader will load .chepak as a resource via ResourceManager.

#include <FileSystem/PakFile.h>

// Type aliases so callers can use CHEngine::AssetPackReader / AssetPackBuilder
namespace CHEngine {
    using AssetPackReader  = PakReader;
    using AssetPackBuilder = PakBuilder;
} // namespace CHEngine
