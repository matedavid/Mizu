#include "uuid.h"

#include <random>

namespace Mizu {

static std::random_device s_random_device;
static std::mt19937 s_rng(s_random_device());
static std::uniform_int_distribution<UUID::Type> s_distribution;

UUID::UUID() : m_value(s_distribution(s_rng)) {}

UUID::UUID(Type value) : m_value(value) {}

// UUID::UUID(UUID& other) : m_value(other.m_value) {}

} // namespace Mizu
