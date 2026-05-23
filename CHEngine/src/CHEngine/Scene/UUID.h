#pragma once
#include <Core.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <iosfwd>
#include <format>

namespace CHEngine {

// Universally unique identifier — 16 bytes, UUID v4 layout.
// All Boost.UUID headers are confined to UUID.cpp — this header is boost-free.
class CHENGINE_API UUID {
public:
    UUID() noexcept;  // creates nil UUID (all zeros)

    // Factory
    static UUID Generate();                     // random v4 UUID
    static UUID Nil() noexcept;                // nil (all-zero) UUID
    // Parses "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx" or "{...}".
    // Returns Nil() on invalid input.
    static UUID FromString(std::string_view s);

    // String conversion  ("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx")
    std::string    ToString() const;
    operator std::string() const { return ToString(); }

    // True if not all-zero
    bool IsValid() const noexcept;

    // Raw bytes (read-only, 16 bytes)
    const uint8_t* Data() const noexcept { return m_Data; }

    bool operator==(const UUID& o) const noexcept;
    bool operator!=(const UUID& o) const noexcept;
    bool operator< (const UUID& o) const noexcept;

private:
    uint8_t m_Data[16]{};
};

inline std::ostream& operator<<(std::ostream& os, const UUID& u) {
    os << u.ToString();
    return os;
}

// ADL hook so boost::hash<UUID> works without pulling boost headers everywhere
CHENGINE_API std::size_t hash_value(const UUID& u);

} // namespace CHEngine

// std::formatter specialization (for std::format("... {}", uuid))
template<>
struct std::formatter<CHEngine::UUID> : std::formatter<std::string> {
    auto format(const CHEngine::UUID& uuid, std::format_context& ctx) const {
        return std::formatter<std::string>::format(uuid.ToString(), ctx);
    }
};

// std::hash specialization (for std::unordered_map<UUID, ...>)
template<>
struct std::hash<CHEngine::UUID> {
    std::size_t operator()(const CHEngine::UUID& u) const noexcept {
        return CHEngine::hash_value(u);
    }
};
