#include "diagnostic_file.hpp"

#include <json/json.hpp>

#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <diagnostic_file.json>\n";
		return 1;
	}

	// Read diagnostic file
	std::ifstream  file(argv[1]);
	nlohmann::json input = nlohmann::json::parse(file);
	file.close();

	// Parse diagnostic thread
	dia_app::dia_file::Thread diagnostic_thread = dia_app::dia_file::Thread::fromJson(input);

	// For now, just print success
	std::cout << "Successfully parsed diagnostic file\n";
	std::cout << "Main message type: " << diagnostic_thread.main_message.metadata.type << "\n";
	std::cout << "Attached messages: " << diagnostic_thread.attached_messages.size() << "\n";

	return 0;
}
