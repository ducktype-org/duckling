bool is_in_box(float x, float y, float p1x, float p1y, float p2x, float p2y) {
    return ((x < p1x && x > p2x) || (x > p1x && x < p2x)) && (y < p1y && y > p2y) || (y > p1y && y < p2y);
}
