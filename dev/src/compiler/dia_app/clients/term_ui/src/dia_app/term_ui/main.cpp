#include <dia_app/view_manager/view_manager.hpp>
#include "view.hpp"

#include <json/json.hpp>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Pass a single file as argument\n";
        return 1;
    }
    
    std::ifstream file(argv[1]);
    nlohmann::json input = nlohmann::json::parse(file);
    file.close();

    
    auto view_manager = dia_app::view_manager::ViewManager::createFromJson(input);
	::view::ViewResponse *vm_data = new ::view::ViewResponse;
	view_manager.getView(vm_data);

	// Format and print the static message to the terminal.
	term_ui::View term_msg(*vm_data);
	term_msg.print(std::cerr, true);

    return 0;
}