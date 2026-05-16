#include "ActionMap.h"

#include <Input/KeyCodes.h>
#include <Log/Log.h>

#include <fstream>
#include <nlohmann/json.hpp>

namespace Sandbox {

namespace {

const std::unordered_map<std::string, int>& KeyNameMap()
{
    static const std::unordered_map<std::string, int> map = {
        { "Space",        CHEngine::Key::Space        },
        { "Apostrophe",   CHEngine::Key::Apostrophe   },
        { "Comma",        CHEngine::Key::Comma        },
        { "Minus",        CHEngine::Key::Minus        },
        { "Period",       CHEngine::Key::Period       },
        { "Slash",        CHEngine::Key::Slash        },
        { "0", CHEngine::Key::D0 }, { "1", CHEngine::Key::D1 },
        { "2", CHEngine::Key::D2 }, { "3", CHEngine::Key::D3 },
        { "4", CHEngine::Key::D4 }, { "5", CHEngine::Key::D5 },
        { "6", CHEngine::Key::D6 }, { "7", CHEngine::Key::D7 },
        { "8", CHEngine::Key::D8 }, { "9", CHEngine::Key::D9 },
        { "Semicolon", CHEngine::Key::Semicolon }, { "Equal", CHEngine::Key::Equal },
        { "A", CHEngine::Key::A }, { "B", CHEngine::Key::B }, { "C", CHEngine::Key::C },
        { "D", CHEngine::Key::D }, { "E", CHEngine::Key::E }, { "F", CHEngine::Key::F },
        { "G", CHEngine::Key::G }, { "H", CHEngine::Key::H }, { "I", CHEngine::Key::I },
        { "J", CHEngine::Key::J }, { "K", CHEngine::Key::K }, { "L", CHEngine::Key::L },
        { "M", CHEngine::Key::M }, { "N", CHEngine::Key::N }, { "O", CHEngine::Key::O },
        { "P", CHEngine::Key::P }, { "Q", CHEngine::Key::Q }, { "R", CHEngine::Key::R },
        { "S", CHEngine::Key::S }, { "T", CHEngine::Key::T }, { "U", CHEngine::Key::U },
        { "V", CHEngine::Key::V }, { "W", CHEngine::Key::W }, { "X", CHEngine::Key::X },
        { "Y", CHEngine::Key::Y }, { "Z", CHEngine::Key::Z },
        { "LeftBracket",  CHEngine::Key::LeftBracket  },
        { "Backslash",    CHEngine::Key::Backslash    },
        { "RightBracket", CHEngine::Key::RightBracket },
        { "GraveAccent",  CHEngine::Key::GraveAccent  },
        { "Escape",       CHEngine::Key::Escape       },
        { "Enter",        CHEngine::Key::Enter        },
        { "Tab",          CHEngine::Key::Tab          },
        { "Backspace",    CHEngine::Key::Backspace    },
        { "Insert",       CHEngine::Key::Insert       },
        { "Delete",       CHEngine::Key::Delete       },
        { "Right",        CHEngine::Key::Right        },
        { "Left",         CHEngine::Key::Left         },
        { "Down",         CHEngine::Key::Down         },
        { "Up",           CHEngine::Key::Up           },
        { "PageUp",       CHEngine::Key::PageUp       },
        { "PageDown",     CHEngine::Key::PageDown     },
        { "Home",         CHEngine::Key::Home         },
        { "End",          CHEngine::Key::End          },
        { "F1",  CHEngine::Key::F1  }, { "F2",  CHEngine::Key::F2  },
        { "F3",  CHEngine::Key::F3  }, { "F4",  CHEngine::Key::F4  },
        { "F5",  CHEngine::Key::F5  }, { "F6",  CHEngine::Key::F6  },
        { "F7",  CHEngine::Key::F7  }, { "F8",  CHEngine::Key::F8  },
        { "F9",  CHEngine::Key::F9  }, { "F10", CHEngine::Key::F10 },
        { "F11", CHEngine::Key::F11 }, { "F12", CHEngine::Key::F12 },
    };
    return map;
}

const std::unordered_map<std::string, int>& MouseNameMap()
{
    static const std::unordered_map<std::string, int> map = {
        { "Left",   CHEngine::MouseButton::Left   },
        { "Right",  CHEngine::MouseButton::Right  },
        { "Middle", CHEngine::MouseButton::Middle },
    };
    return map;
}

uint8_t ParseMods(const nlohmann::json& arr)
{
    uint8_t m = Mod_None;
    if (!arr.is_array()) return m;
    for (const auto& v : arr)
    {
        if (!v.is_string()) continue;
        const std::string s = v.get<std::string>();
        if      (s == "Ctrl"  || s == "Super" || s == "Cmd") m |= Mod_Ctrl;
        else if (s == "Shift")                               m |= Mod_Shift;
        else if (s == "Alt")                                 m |= Mod_Alt;
    }
    return m;
}

TriggerType ParseTrigger(const std::string& s)
{
    if (s == "Down")     return TriggerType::Down;
    if (s == "Released") return TriggerType::Released;
    if (s == "Drag")     return TriggerType::Drag;
    return TriggerType::Pressed;
}

InputContext ParseContext(std::string_view fullName)
{
    if (fullName.substr(0, 5) == "Game.") return InputContext::Game;
    return InputContext::Editor;
}

} // namespace

void ActionMap::Clear()
{
    m_Actions.clear();
}

bool ActionMap::LoadFromJson(const std::filesystem::path& path)
{
    Clear();

    std::ifstream f(path);
    if (!f.is_open())
    {
        CHE_CORE_WARN("InputActions: failed to open {}", path.string());
        return false;
    }

    nlohmann::json j;
    try { f >> j; }
    catch (const std::exception& e)
    {
        CHE_CORE_ERROR("InputActions: JSON parse error in {}: {}", path.string(), e.what());
        return false;
    }

    if (!j.is_object())
    {
        CHE_CORE_ERROR("InputActions: top-level JSON must be an object ({})", path.string());
        return false;
    }

    const auto& keys = KeyNameMap();
    const auto& mice = MouseNameMap();

    for (auto it = j.begin(); it != j.end(); ++it)
    {
        const std::string& name = it.key();
        if (!it.value().is_array()) continue;

        Action action;
        action.fullName = name;
        action.context  = ParseContext(name);

        for (const auto& b : it.value())
        {
            ActionBinding ab{};
            if (b.contains("key") && b["key"].is_string())
            {
                auto kit = keys.find(b["key"].get<std::string>());
                if (kit != keys.end()) ab.key = kit->second;
            }
            if (b.contains("mouse") && b["mouse"].is_string())
            {
                auto mit = mice.find(b["mouse"].get<std::string>());
                if (mit != mice.end()) ab.mouseButton = mit->second;
            }
            if (b.contains("mods"))
                ab.mods = ParseMods(b["mods"]);
            if (b.contains("trigger") && b["trigger"].is_string())
                ab.trigger = ParseTrigger(b["trigger"].get<std::string>());
            if (b.contains("drag_threshold") && b["drag_threshold"].is_number())
                ab.dragThreshold = b["drag_threshold"].get<float>();

            if (ab.key < 0 && ab.mouseButton < 0) continue;
            action.bindings.push_back(ab);
        }

        if (!action.bindings.empty())
            m_Actions.emplace(name, std::move(action));
    }

    CHE_CORE_INFO("InputActions: loaded {} actions from {}", m_Actions.size(), path.string());
    return true;
}

const Action* ActionMap::Find(std::string_view fullName) const
{
    auto it = m_Actions.find(std::string(fullName));
    return it != m_Actions.end() ? &it->second : nullptr;
}

} // namespace Sandbox
