#include <array>
#include <iostream>
#include <json/json.hpp>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

struct Foo {
	int         a;
	char        b;
	std::string s;

	JS_OBJ(a, b, s);
};

struct Bar {
	std::string      a;
	std::vector<int> g;
	char             b;
	float            s;

	JS_OBJ(a, g, b, s);
};

struct Empty {};

REGISTER_PARSE_TYPE(Foo);
REGISTER_PARSE_TYPE(Bar);
REGISTER_PARSE_TYPE(Empty);

struct Fiz {
	int x;

	JS_OBJ(x);
};

REGISTER_PARSE_TYPE(Fiz);

int main() {
	using MyVar = std::variant<Foo, Bar, Empty>;

	Foo   foo{ 5, 'a', "abc" };
	Bar   bar{ "abc", { 3, 4 }, 'b', 4.1f };
	MyVar x{ Empty{} };
	MyVar y{ foo };
	MyVar z{ bar };

	std::cout << JS::serializeStruct(x) << "\n";
	std::cout << JS::serializeStruct(y) << "\n";
	std::cout << JS::serializeStruct(z) << "\n";

	std::exception* e = new std::runtime_error("error");

	std::cout << JS::serializeStruct(*e) << "\n";
}
