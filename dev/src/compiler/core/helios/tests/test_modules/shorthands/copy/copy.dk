# A class that owns a `box`, which makes it non-trivially-copyable. The box field has an
# initializer so the class stays default-constructible (and thus so does the `holder` var below).
class HasBox {
    boxed: box i32 = 0;
}

var holder: HasBox;
