#include "entry_point.h"

#include "core/application.h"

#ifndef MIZU_NO_ENTRY_POINT

int main(int argc, const char* argv[]) {
    Mizu::Application* app = create_application(argc, argv);
    if (app == nullptr)
        return 1;

    app->run();
    delete app;

    return 0;
}

#endif