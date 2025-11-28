#include "logger.hpp"

namespace dia_int {

	void Logger::dumpLog(std::ostream& out) {
		for (auto& msg: diagnostics) {
			try {
				auto diagnostic_file = msg->buildDiagnosticFile();
				// std::cout << diagnostic_file.toJson().dump(4) << "\n\n";
				auto state = dia_app::evaluateDiagnostic(diagnostic_file);
				// state.debugPrint(std::cout);
				auto view  = dia_app::term_ui_view::constructTreeView(state);
				term_ui::print(view, out);
			} catch (const std::exception& e) {
				out << "Error while printing diagnostic message: \n" << e.what() << "\n";
			}
		}
		diagnostics.clear();
	}

	void Logger::log(Box<DiagnosticBase> message) { diagnostics.push_back(std::move(message)); }
}
