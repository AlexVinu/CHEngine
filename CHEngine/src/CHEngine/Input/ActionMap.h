#pragma once

#include "ActionBinding.h"
#include "InputContext.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CHEngine {

struct InputAction {
    InputContext               context;    // e.g. "Editor", "Game", "Vehicle"
    std::string                fullName;   // e.g. "Editor.Gizmo.Translate"
    std::vector<ActionBinding> bindings;
};

class ActionMap {
public:
    void Clear();
    bool LoadFromJson(const std::filesystem::path& path);

    const InputAction* Find(std::string_view fullName) const;

    const std::unordered_map<std::string, InputAction>& All() const { return m_Actions; }

private:
    std::unordered_map<std::string, InputAction> m_Actions;
};

} // namespace CHEngine
