#pragma once

#include "pch.hpp"
#include "NodeEditor.hpp"

namespace Logicarium {
    class Logicarium
    {
    public:
        static void glfw_error_callback(int error, const char* description);

        Logicarium();
        int Mainloop();
    };
}
