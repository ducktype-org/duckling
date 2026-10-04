#include <json/json.hpp>

#include <iostream>
#include <string>
#include <variant>
#include <vector>

struct Foo {
	int         a;
	char        b;
	std::string s;

	// Specify which fields are supposed to be JSONed
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Foo, a, b, s);
};
// Tell our library about this struct
JSON_REGISTER_TYPE(Foo)

struct Bar {
	std::string      a;
	std::vector<int> g;
	char             b;
	float            s;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Bar, a, g, b, s);
};
// We can also specify a custom name
JSON_REGISTER_TYPE_WITH_NAME(Bar, "Barbara")

struct Empty {};
JSON_REGISTER_TYPE(Empty)

int main() {
	using MyVar = std::variant<Foo, Bar, Empty, std::string>;

	Foo   foo{ .a = 5, .b = 'a', .s = "abc" };
	Bar   bar{ .a = "abc", .g = { 3, 4 }, .b = 'b', .s = 4.1f };
	MyVar x{ Empty{} };
	MyVar y{ foo };
	MyVar z{ bar };


	nlohmann::json j(x);
	std::cout << j << "\n";
	j = y;
	std::cout << j << "\n";
	j = z;
	std::cout << j << "\n";

	std::exception* e = new std::runtime_error("error");
	std::cout << nlohmann::json(*e) << "\n";
	delete e;

	Empty empty;
	std::cout << nlohmann::json(empty) << '\n';
}
