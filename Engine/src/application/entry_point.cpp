#include "application/main_loop.h"

using namespace Mizu;

int main()
{
    MainLoop main_loop{};

    if (!main_loop.init())
        return 1;

    main_loop.run();

    return 0;
}
