#pragma once

#include "ActionBinding.h"
#include "InputContext.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Sandbox {

struct Action {
    InputContext               context = InputContext::Editor;
    std::string                fullName;   // e.g. "Editor.Gizmo.Translate"
    std::vector<ActionBinding> bindings;
};

class ActionMap {
public:
    void Clear();
    bool LoadFromJson(const std::filesystem::path& path);

    const Action* Find(std::string_view fullName) const;

    const std::unordered_map<std::string, Action>& All() const { return m_Actions; }

private:
    std::unordered_map<std::string, Action> m_Actions;
};

} // namespace Sandbox
