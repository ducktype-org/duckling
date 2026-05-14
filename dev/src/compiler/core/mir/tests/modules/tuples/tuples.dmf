const Point = (i64, i64);
const Square = (Point, Point, Point, Point);

fun make_square(origin: Point, side: i64) -> Square = {
    var sw = origin;
    var nw = (sw._1, sw._2 + side);
    var ne = (nw._1 + side, nw._2);
    var se = (ne._1, ne._2 - side);

    return (sw, nw, ne, se);
}
