#include "view_manager.hpp"

#include <json/json.hpp>

#include <fstream>

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cerr << "Pass a single file as argument\n";
		return 1;
	}

	std::ifstream  file(argv[1]);
	nlohmann::json input = nlohmann::json::parse(file);
	file.close();

	dia_app::view_manager::runViewManagerRPCServer(input);

	return 0;
}
