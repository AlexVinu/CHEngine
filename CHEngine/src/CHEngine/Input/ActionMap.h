#pragma once

#include "ActionBinding.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace CHEngine {

struct InputAction {
    std::string                fullName;   // e.g. "Editor.Gizmo.Translate"
    std::vector<ActionBinding> bindings;
};

class ActionMap {
public:
    void Clear();

    // Загрузить один файл под заданным контекстом.
    // Ключи внутри JSON — короткие (без префикса контекста).
    // fullName композируется как "<ctx>.<key>".
    bool LoadFromJson(std::string_view context, const std::filesystem::path& path);

    // Сканирует dir/*.json; stem имени файла = контекст.
    bool LoadFromDirectory(const std::filesystem::path& dir);

    // Поиск по композированному fullName ("Editor.Gizmo.Translate").
    const InputAction* Find(std::string_view fullName) const;

    // Вернуть inner-map для контекста или nullptr, если контекст не загружен.
    const std::unordered_map<std::string, InputAction>* All(std::string_view context) const;

    // Список загруженных контекстов.
    std::vector<std::string> Contexts() const;

private:
    std::unordered_map<std::string, std::unordered_map<std::string, InputAction>> m_Actions;
};

} // namespace CHEngine
