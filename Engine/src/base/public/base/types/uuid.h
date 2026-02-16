#pragma once

#include <cstdint>
#include <functional>
#include <stddef.h>

#include "mizu_base_module.h"

namespace Mizu
{

class MIZU_BASE_API UUID
{
  public:
    using Type = size_t;

    UUID();
    explicit UUID(Type value);

    UUID(const UUID&) = default;
    UUID& operator=(const UUID&) = default;

    static UUID invalid() { return UUID(static_cast<Type>(0)); }

    bool is_valid() const { return m_value != 0; }

    bool operator==(const UUID& other) const { return m_value == other.m_value; }
    bool operator<(const UUID& other) const { return m_value < other.m_value; }

    operator Type() const { return m_value; }
    operator bool() const { return is_valid(); }

  private:
    Type m_value = 0;
};

} // namespace Mizu

template <>
struct std::hash<Mizu::UUID>
{
    Mizu::UUID::Type operator()(const Mizu::UUID& k) const { return static_cast<Mizu::UUID::Type>(k); }
};
