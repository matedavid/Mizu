#include <catch2/catch_session.hpp>

#include "Mizu/Mizu.h"

Mizu::Application* create_application(int argc, const char* argv[]) {
    Catch::Session().run(argc, argv);
    return nullptr;
}