#include "cli.hpp"
#include "common.hpp"

#include <mutex>

namespace vm::debugger::cli {
	namespace impl {
		struct ImplementationSpecific {
			std::mutex output_mutex;
			bool       running = false;
		};
	}

	CLIDebugger::CLIDebugger(): spec(new impl::ImplementationSpecific()) {}

	CLIDebugger::~CLIDebugger() = default;

	void CLIDebugger::mainLoop(clah::Clah& clah) {
		events::Listener<api::ProcStatus> status_change_listener([&](const api::ProcStatus& status) {
			printer::PrinterOStream out;
			out << "New status: " << common::typeToString(status);
			// printProcStatus(out, status);
			for (std::string& value: common::extractPrimitiveValues(status))
				out << " (return value = " << value << ")";

			printNL(out.getContents());
		});

		debugger.attachOnStatusChangedListener(status_change_listener);

		printNL("Welcome to BeRD - an interactive in-DVM debugger!");

		spec->running = true;
		for (std::string line; spec->running && std::getline(std::cin, line);
		     clah.execute(common::strip(line)));

		printNL("Exiting debugger.");
	}

	void CLIDebugger::exitMainLoop() { spec->running = false; }

	void CLIDebugger::print(const printer::PrinterContentsSeq& content) {
		std::lock_guard lk(spec->output_mutex);
		printer::StreamPrinter::print(content, std::cout);
	}
}
