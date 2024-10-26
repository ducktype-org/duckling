#include <variant>
#include <vector>
#include <iostream>
 
double doubleSum(std::vector<std::variant<int, double>> list) {
    double out = 0;
    for (auto elem: list) {
        std::visit([&](auto arg) {
            out += arg;
        }, elem);
    }

    return out;
}

int main() {
    std::cout << doubleSum({1, 2.5, 3, 4.5}) << "\n";
}
