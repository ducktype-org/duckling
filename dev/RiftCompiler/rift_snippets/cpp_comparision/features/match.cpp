#include <iostream>

struct Point {
    int x, y;
};

int main() {
    Point p;
    p.x = 4;
    p.y = 5;

    if (p.x == 0 && p.y == 0)
        std::cout << "Center point\n";
    else if (p.y == 0) 
        std::cout << "Point on the x axis, with x coord = " << p.x << "\n";
    else if (p.x == 0)
        std::cout << "Point on the x axis, with x coord = " << p.y << "\n";
    else
        std::cout << "Point with coords = (" << p.x << p.y << ")\n";
}
