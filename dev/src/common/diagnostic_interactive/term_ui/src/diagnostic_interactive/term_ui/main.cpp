#include "view.hpp"

#include <dia_app/view_manager/view_manager.hpp>

#include <json/json.hpp>

#include <fstream>

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cerr << "Pass a single file as argument\n";
		return 1;
	}

	// Parse the input file to JSON object.
	std::ifstream  file(argv[1]);
	nlohmann::json input = nlohmann::json::parse(file);
	file.close();

	// Create a view manager and initialize it with the parsed JSON.
	auto                  view_manager = dia_app::view_manager::ViewManager::createFromJson(input);
	::view::ViewResponse* vm_data      = new ::view::ViewResponse;
	view_manager.getView(vm_data);

	// Format and print the static message to the terminal.
	term_ui::View term_msg(*vm_data);
	bool          use_color = true;
	term_msg.print(std::cerr, use_color);

	return 0;
}
