#include "../init_guard.hpp"
#include <iostream>

namespace base::detail {
    void logInitFunction(const char* function_name) {
        // @TODO: Wrap it into some generic logger
        // module, that we will use compiler-wide:
        std::cerr << "Init: " << function_name << '\n';
    }
}
