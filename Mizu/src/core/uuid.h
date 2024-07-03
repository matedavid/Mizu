#pragma once

#include <cstdint>
#include <functional>
#include <stddef.h>

namespace Mizu {

class UUID {
  public:
    using Type = size_t;

    UUID();
    UUID(Type value);
    // UUID(UUID& other);

    [[nodiscard]] bool is_valid() const { return m_value != 0; }

    bool operator==(const UUID& other) const { return m_value == other.m_value; }
    bool operator<(const UUID& other) const { return m_value < other.m_value; }

    [[nodiscard]] operator Type() const { return m_value; }
    [[nodiscard]] operator bool() const { return is_valid(); }

  private:
    Type m_value = 0;
};

} // namespace Mizu

template <>
struct std::hash<Mizu::UUID> {
    Mizu::UUID::Type operator()(const Mizu::UUID& k) const { return static_cast<Mizu::UUID::Type>(k); }
};
