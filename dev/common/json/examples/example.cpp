#include <variant>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <json/json.hpp>

struct Foo {
	int         a;
	char        b;
	std::string s;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Foo, a, b, s);
};

struct Bar {
	std::string      a;
	std::vector<int> g;
	char             b;
	float            s;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Bar, a, g, b, s);
};

struct Empty {};

NLOHMANN_EMPTY_STRUCT(Empty);

struct Fiz {
	int x;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Fiz, x);
};

int main() {
	using MyVar = std::variant<Foo, Bar, Empty>;

	Foo   foo{ 5, 'a', "abc" };
	Bar   bar{ "abc", { 3, 4 }, 'b', 4.1f };
	MyVar x{ Empty{} };
	MyVar y{ foo };
	MyVar z{ bar };

	nlohmann::json j;
	nlohmann::to_json(j, x);

	// std::cout << j << "\n";
	// std::cout << JS::serializeStruct(y) << "\n";
	// std::cout << JS::serializeStruct(z) << "\n";

	std::exception* e = new std::runtime_error("error");

	// std::cout << JS::serializeStruct(*e) << "\n";
}
