class Point { x: i64 = 0; y: i64 = 0; }
class Polygon { vertices: Point[3]; id: i64 = 0; }

fun main() -> i64 = {
    var points: Point[2];
    points[1] = Point(30, 40);
    builtin_output_i64(points[1].y); # 40

    var poly: Polygon;
    poly.vertices[2] = Point(5, 6);
    builtin_output_i64(poly.vertices[2].y); # 6

    var matrix: i32[2][2];
    matrix[1][0] = 777;
    builtin_output_i64(matrix[1][0]); # 777

    return 0;
}
