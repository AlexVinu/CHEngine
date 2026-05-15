#pragma once

// Shared popup state — ensures only one popup is open at a time.
// Each popup checks g_OpenPopup before opening and clears it on close.
namespace EditorPopup {

enum class ID { None, ShiftA, ShiftW };

inline ID g_Open = ID::None;

inline void Open(ID id)  { g_Open = id; }
inline void Close()      { g_Open = ID::None; }
inline bool IsOpen(ID id){ return g_Open == id; }
inline bool AnyOpen()    { return g_Open != ID::None; }

} // namespace EditorPopup
