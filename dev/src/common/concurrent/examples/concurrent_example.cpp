#include "hide.hpp"

int main() {
    for (int i = 0; i < 1'000'000; i++) {
        allocateHide();
    }
}
