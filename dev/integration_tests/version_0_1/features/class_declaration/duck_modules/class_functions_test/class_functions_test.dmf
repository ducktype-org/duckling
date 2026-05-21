# Test functions operating on classes
# Focus: passing classes to functions, returning classes from functions

class Point {
    x: i64 = 0;
    y: i64 = 0;
}

class Vector3 {
    x: i64 = 0;
    y: i64 = 0;
    z: i64 = 0;
}

# Functions that take classes as parameters
fun point_magnitude_squared(p: Point) -> i64 = p.x * p.x + p.y * p.y;

fun point_distance_squared(p1: Point, p2: Point) -> i64 = {
    let dx = p2.x - p1.x;
    let dy = p2.y - p1.y;
    return dx * dx + dy * dy;
}

# Functions that return classes
fun point_add(p1: Point, p2: Point) -> Point = Point(p1.x + p2.x, p1.y + p2.y);

fun point_scale(p: Point, s: i64) -> Point = Point(p.x * s, p.y * s);

fun make_point(x: i64, y: i64) -> Point = Point(x, y);

# Function with class and primitives mixed
fun point_offset(p: Point, dx: i64, dy: i64) -> Point = Point(p.x + dx, p.y + dy);

# Vector operations
fun vector_dot(v1: Vector3, v2: Vector3) -> i64 = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;

fun main() -> i64 = {
    # Test function taking class as parameter
    var p1 = Point(3, 4);
    builtin_output_i64(point_magnitude_squared(p1));  # 25

    # Test function with two class parameters
    var p2 = Point(0, 0);
    var p3 = Point(3, 4);
    builtin_output_i64(point_distance_squared(p2, p3));  # 25

    # Test function returning class
    var p4 = point_add(Point(1, 2), Point(3, 4));
    builtin_output_i64(p4.x);              # 4
    builtin_output_i64(p4.y);              # 6

    # Test chained function calls with classes
    var p5 = point_scale(Point(2, 3), 3);
    builtin_output_i64(p5.x);              # 6
    builtin_output_i64(p5.y);              # 9

    # Test factory function
    var p6 = make_point(10, 20);
    builtin_output_i64(p6.x);              # 10
    builtin_output_i64(p6.y);              # 20

    # Test mixed class and primitive parameters
    var p7 = point_offset(Point(5, 5), 3, 7);
    builtin_output_i64(p7.x);              # 8
    builtin_output_i64(p7.y);              # 12

    # Test Vector3 dot product
    var v1 = Vector3(1, 2, 3);
    var v2 = Vector3(4, 5, 6);
    builtin_output_i64(vector_dot(v1, v2)); # 1*4 + 2*5 + 3*6 = 32

    # Test nested function calls
    var result = point_magnitude_squared(point_add(Point(0, 0), Point(5, 12)));
    builtin_output_i64(result);            # 169

    return 0;
}
