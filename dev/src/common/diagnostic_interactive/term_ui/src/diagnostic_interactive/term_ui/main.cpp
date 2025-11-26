#include <diagnostic_interactive/term_ui/printers.hpp>
#include <diagnostic_interactive/core/template_registry.hpp>
#include <diagnostic_interactive/core/diagnostic_file.hpp>
#include <diagnostic_interactive/core/view_constructors.hpp>
#include <diagnostic_interactive/core/template_evaluation.hpp>
#include <json/json.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <diagnostic_file>\n";
        return 1;
    }

    std::string file_path = argv[1];

    // Set template registry to embedded templates
    dia_app::TemplateRegistry::setInstance(
        makeBox<dia_app::TemplateResistryEmbeddedProvider>()
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
        auto j = nlohmann::json::parse(content);
        auto thread = dia_app::dia_file::Thread::fromJson(j);

        // Create a tree view constructor and display it in the term ui
        auto state = dia_app::evaluateDiagnostic(thread);
        auto view = dia_app::term_ui_view::constructTreeView(state);

        term_ui::print(view, std::cout);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
