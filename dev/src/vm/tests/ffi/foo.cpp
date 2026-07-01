#include <iostream>

extern "C" {
    void foo() {
        std::cout << "Hello from FFI!\n";
    }
}
