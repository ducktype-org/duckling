const Person: type = (str, str, i64); # name, surname, age

fun makePerson(name: str, surname: str, age: i64) -> Person = {
    return (name, surname, age);
}

fun getName(p: Person) = {
    return p._1;
}

fun getSurname(p: Person) = {
    return p._2;
}

fun getAge(p: Person) = {
    return p._3;
}

fun main() = {
    var p = makePerson("John", "Doe", 30);
    var name = getName(p);
    var surname = getSurname(p);
    var age = getAge(p);

    # builtin_output_string(name);
    # builtin_output_string(surname);
    # builtin_output_i64(age);

    p._1 = "Jane";
    p._2 = "Smith";
    p._3 = 18 + 5;

    # builtin_output_string(p._1);
    # builtin_output_string(p._2);
    # builtin_output_i64(p._3);

    return 0;
}
