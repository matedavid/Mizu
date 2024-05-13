#pragma once

namespace Mizu {
class Application;
} // namespace Mizu

#ifndef MIZU_NO_ENTRY_POINT

extern Mizu::Application* create_application(int argc, const char* argv[]);

#endif