#include "cli.hpp"

namespace {
	std::string strip(std::string& string) {
		string.erase(0, string.find_first_not_of(" \t\n\r"));
		string.erase(string.find_last_not_of(" \t\n\r") + 1);
		return string;
	}

	template<typename T>
	std::string typeToString(const T& status) {
		return std::visit(
			[&](auto&& arg) {
				using TT = std::decay_t<decltype(arg)>;
				return TypeParseTraits<TT>::NAME.data();
			},
			status
		);
	}

	void printProcStatus(printer::PrinterOStream& os, const vm::api::ProcStatus& status) {
		os.add(printer::PrinterContent(typeToString(status)));
		if (v_matches(status, vm::api::ExecutionCompleted)) {
			const auto& exit_value = std::get<vm::api::ExecutionCompleted>(status).exit_value;
			if (v_matches(exit_value, std::vector<Ref<vm::IVMValue>>)) {
				for (auto val: std::get<std::vector<Ref<vm::IVMValue>>>(exit_value)) {
					if_opt_some(val->readData(), data) {
						variant_match(data) {
							variant_case(vm::interpreted_data_variant::Primitive, primitive) {
								os << " (return value = " << std::to_string(primitive.value) << ")";
							}
						}
					}
				}
			}
		}
	}
}

namespace vm::debugger::cli {
	void CLIDebugger::mainLoop(clah::Clah& clah) {
		events::Listener<api::ProcStatus> status_change_listener([&](const api::ProcStatus& status) {
			printer::PrinterOStream out;
			out << "New status: ";
			printProcStatus(out, status);
			printNL(out.getContents());
		});

		events::Listener<std::string> error_listener([&](const std::string& err) {
			printError(err);
		});

		events::Listener<std::string> output_listener([&](const std::string& str) {
			print({ { str, printer::Color::BrightCyan } });
		});

		debugger.attachOnStatusChangedListener(status_change_listener);
		debugger.attachOnErrorListener(error_listener);
		debugger.attachOnOutputListener(output_listener);

		printNL(
			"++++++++++++++++++++++++++++\n"
			"+   Debugger has started   +\n"
			"++++++++++++++++++++++++++++"
		);

		spec.running = true;
		for (std::string line; spec.running && std::getline(std::cin, line);
		     clah.execute(strip(line)));

		printNL("Exiting debugger.");
	}

	void CLIDebugger::exitMainLoop() { spec.running = false; }

	void CLIDebugger::print(const printer::PrinterContentsSeq& content) {
		std::lock_guard lk(spec.output_mutex);
		printer::StreamPrinter::print(content, std::cout);
	}
}
