#include <iostream>

struct point {
    float x, y;
};

struct circle {
    point m;
    float r;
};

bool is_in_circle (point p, circle c) {
    float d2 = (p.x - c.m.x)*(p.x - c.m.x) + (p.y - c.m.y)*(p.x - c.m.y);
    return d2 < c.r*c.r;
}

int main() {
    circle myCircle;
    myCircle.m.x = 0;
    myCircle.m.y = 0;
    myCircle.r = 1;

    point myPoint;
    myPoint.x = 0.5;
    myPoint.y = -0.1;

    std::cout << is_in_circle(myPoint, myCircle);

    return 0;
}
