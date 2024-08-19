#include <variant>
#include <iostream>
#include <string>
#include <vector>

#include <json/json.hpp>

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
// An empty struct has a special register macro
JSON_REGISTER_EMPTY_STRUCT(Empty)

struct Empty2 {};
// It also comes with a name variant
JSON_REGISTER_EMPTY_STRUCT_WITH_NAME(Empty2, "BetterEmpty")

int main() {
	using MyVar = std::variant<Foo, Bar, Empty>;

	Foo   foo{ 5, 'a', "abc" };
	Bar   bar{ "abc", { 3, 4 }, 'b', 4.1f };
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
