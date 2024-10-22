#include <iostream>

int main() {
    std::pair<int, int> p = {4, 5};

    if (p.first == 0 && p.second == 0)
        std::cout << "Center point\n";
    else if (p.second == 0) 
         std::cout << "Point on the x axis, with x coord = " << p.first << "\n";
    else if (p.first == 0)
         std::cout << "Point on the x axis, with x coord = " << p.second << "\n";
    else
         std::cout << "Point with coords = (" << p.first << p.second << ")\n";
}
