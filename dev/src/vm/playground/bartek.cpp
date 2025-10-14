#include <cstddef>
int add(int a, int b) {
    return a + b;
}

int main () {
    std::byte* (*function_pointer)(std::byte*) = nullptr;
}
