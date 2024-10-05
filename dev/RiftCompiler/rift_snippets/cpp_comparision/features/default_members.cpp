#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

class Point {
public:
    int x, y;

    // Domyślny konstruktor
    Point() : x(0), y(0) {}

    // Konstruktor z parametrami
    Point(int x, int y) : x(x), y(y) {}

    // Operator porównania ==
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }

    // Operator porównania <
    bool operator<(const Point& other) const {
        return (x < other.x) || (x == other.x && y < other.y);
    }

    // Metoda do obliczania odległości od punktu
    double distanceFrom(const Point& other) const {
        return std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
    }

    // Funkcja wyświetlająca punkt
    void display() const {
        std::cout << "(" << x << ", " << y << ")";
    }
};

// Funkcja do znajdowania punktu najbliższego do danego
Point findClosestPoint(const Point& reference, const std::vector<Point>& points) {
    Point closestPoint = points[0];
    double closestDistance = reference.distanceFrom(closestPoint);

    for (const auto& point : points) {
        double currentDistance = reference.distanceFrom(point);
        if (currentDistance < closestDistance) {
            closestPoint = point;
            closestDistance = currentDistance;
        }
    }

    return closestPoint;
}

int main() {
    // Tworzenie punktów
    Point p1(3, 4);
    Point p2(1, 2);
    Point p3(5, 6);
    Point p4(1, 2);

    // Porównywanie punktów
    if (p1 == p2) {
        std::cout << "p1 jest równy p2" << std::endl;
    } else {
        std::cout << "p1 nie jest równy p2" << std::endl;
    }

    if (p2 == p4) {
        std::cout << "p2 jest równy p4" << std::endl;
    }

    // Sortowanie punktów
    std::vector<Point> points = {p1, p2, p3, p4};
    std::sort(points.begin(), points.end());

    std::cout << "Posortowane punkty:" << std::endl;
    for (const auto& point : points) {
        point.display();
        std::cout << std::endl;
    }

    // Znalezienie najbliższego punktu do p1
    Point closestPoint = findClosestPoint(p1, points);
    std::cout << "Najbliższy punkt do ";
    p1.display();
    std::cout << " to ";
    closestPoint.display();
    std::cout << std::endl;

    return 0;
}
