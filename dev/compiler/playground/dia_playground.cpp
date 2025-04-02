#include <diagnostic/interactive_message.hpp>
#include <iostream>
#include <json/json.hpp>

using nlohmann::json;

int main() {
	init::InitObject _;
	json             j = dia::ExampleMessage{};

	std::cout << j.dump() << std::endl;
	return 0;
}
