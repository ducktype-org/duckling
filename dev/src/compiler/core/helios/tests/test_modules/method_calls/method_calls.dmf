class ExampleClass {
    exampleField: i64 = 0;

    fun getField() -> i64 = {
        return exampleField;
    }

    fun setField(field:i64 = 0) = {
        exampleField = field;
    }

    fun setToZero() = {
        setField(0);
    }
}

class Wrapper {
    innerField: ExampleClass;

    fun getInnerViaCall() -> i64 = {
        return innerField.getField();
    }

    fun getInnerViaAccess() -> i64 = {
        return innerField.exampleField;
    }

    fun getInner() -> ExampleClass = {
        return innerField;
    }

    fun setInner(inner: ref ExampleClass) = {
        innerField = inner;
    }
}

class Point {
    x: i64 = 0;
    y: i64 = 0;

    fun manhattanFromOrigin() = {
        return x + self.y;
    }

    fun areaFromOrigin() = {
        return self.x * y;
    }

    fun manhattanFromOrigin(a: i64) = {
        return x + self.y + a;
    }

    fun areaFromOrigin(a: i64) = {
        return self.x * y * a;
    }

    fun setX(newX: i64) = {
        x = newX;
    }

    fun setY(newY: i64) = {
        self.y = newY;
    }
}

fun main() -> i64 = {
    var obj1 = ExampleClass();
    var obj2 = ExampleClass(1);

    var v1 = obj1.exampleField;
    var v2 = obj2.getField();

    # builtin_output_i64(v1); # 0
    # builtin_output_i64(v2); # 1

    obj1.setField(2);
    obj2.setField(3);

    var v3 = obj1.getField(); # 2
    var v4 = obj2.exampleField; # 3

    # builtin_output_i64(v3);
    # builtin_output_i64(v4);

    var wrap_obj = Wrapper(obj1);
    var v5 = wrap_obj.getInnerViaCall();
    wrap_obj.innerField.setToZero();
    var v6 = wrap_obj.getInnerViaAccess();

    # builtin_output_i64(v5); # 2
    # builtin_output_i64(v6); # 0

    return 0;
}
