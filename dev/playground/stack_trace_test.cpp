#include <stacktrace>
#include <iostream>

int main() {
    std::string stack_trace = std::to_string(std::stacktrace::current());
    std::cout << stack_trace << '\n';
    return 0;
}
