#include <diagnostic_interactive/core/diagnostic_arguments.hpp>
#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <diagnostic_interactive/core/template_registry.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>
#include <diagnostic_interactive/term_ui/printers.hpp>

#include <json/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <diagnostic_file>\n";
		return 1;
	}

	std::string file_path = argv[1];

	dia_int::TemplateRegistrySingleton::setInstance(makeBox<dia_int::TemplateResistryMainProvider>()
	);

	// Read the diagnostic file
	std::ifstream f(file_path);
	if (!f.is_open()) {
		std::cerr << "Failed to open file: " << file_path << '\n';
		return 1;
	}
	std::stringstream buffer;
	buffer << f.rdbuf();
	std::string content = buffer.str();

	try {
		auto j      = nlohmann::json::parse(content);
		auto thread = dia_int::dia_args::Diagnostic::fromJson(j);

		// Create a tree view constructor and display it in the term ui
		auto state = dia_int::evaluateDiagnostic(thread);
		auto view  = dia_int::constructTreeView(state);

		term_ui::print(view, std::cout);
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << '\n';
		return 1;
	}

	return 0;
}
