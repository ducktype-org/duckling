const SIZE = 3;

class Point {
    x: i32 = 0;
    y: i32 = 0;
}

class Polygon {
    vertices: Point[SIZE];
}

fun test_complex_indexing() -> i32 = {
    var matrix: i32[SIZE][2];
    matrix[0][1] = 42;

    var poly: Polygon;
    poly.vertices[1].x = 100;

    let idx = 1;
    return matrix[idx][0] + poly.vertices[idx].x;
}
