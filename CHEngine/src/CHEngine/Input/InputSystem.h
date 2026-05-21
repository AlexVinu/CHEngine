#pragma once
#include <CheStl/MemoryTypes.h>

#include "ActionMap.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CHEngine {

    class Window;

// Centralised input → action mapping.  Owns all state as member fields (RAII).
// Принадлежит Application (см. Application::InputSystem()).
class CHENGINE_API InputSystem {
public:
    InputSystem(Ref<Window>);
    ~InputSystem() = default;

    InputSystem(const InputSystem&)            = delete;
    InputSystem& operator=(const InputSystem&) = delete;

    // Загружает все *.json из каталога; stem каждого файла = контекст.
    bool LoadFromDirectory(const std::filesystem::path& dir);

    void BeginFrame();

    // Use json name with your bindings
    void PushContext(std::string_view ctx);
    void PopContext (std::string_view ctx);

    bool IsActiveContext(std::string_view ctx) const;

    bool  Triggered(std::string_view action) const;
    bool  Down     (std::string_view action) const;
    bool  Released (std::string_view action) const;

    // Raw key polling — bypasses action bindings, uses Key::* codes.
    bool  IsKeyDown    (int key) const;
    bool  IsKeyPressed (int key) const;
    bool  IsKeyReleased(int key) const;

    bool  IsModifierDown(uint8_t mods) const;

    enum class Axis { MouseDeltaX, MouseDeltaY, MouseWheel };
    float GetAxis(Axis axis) const;

    void SetMouse(bool active);
    bool IsMouse() const;
private:
	ActionMap                              m_Map;
	std::vector<std::string>              m_ActiveContexts;

    std::unordered_map<std::string, bool>  m_Triggered;
    std::unordered_map<std::string, bool>  m_Down;
    std::unordered_map<std::string, bool>  m_Released;

    float   m_MouseDX     = 0.0f;
    float   m_MouseDY     = 0.0f;
    float   m_MouseWheel  = 0.0f;
    uint8_t m_CurrentMods = 0;

    Ref<Window> m_Window;
};

} // namespace CHEngine
