#include <concurrent/module_flags/worker_count.hpp>
#include <querylinger/queries/version.hpp>

#include <clah/clah.hpp>
#include <filesystem/file.hpp>
#include <init/init.hpp>
#include <printer/stream_printer.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/module_flags/module_flags.hpp>

#include <exception>
#include <iostream>
#include <regex>
#include <thread>

void showVersion() { std::cout << ql::QUERYLINGER_VERSION << '\n'; }

clah::Clah getQuerylingerClah() {
	return clah::Clah("Querylinger", "The query-framework-based Duckling benchmark compiler.")
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Shows version and config")
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('j')
	             .addLongName("threads")
	             .addShortDesc("Number of query worker threads (default: hardware concurrency).")
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::StringParser::make("path"))
	             .addShortName('o')
	             .addLongName("output")
	             .addShortDesc("Output executable path (default: querylinged.out).")
	             .build())
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('i')
	             .addLongName("interactive")
	             .addShortDesc("After the initial compile, read source paths from stdin and "
	                           "recompile incrementally (only changed functions).")
	             .build())
	    .setPreHandler([](const clah::ParsingResult& options) {
			if (options.isFlag("version")) {
				showVersion();
				throw clah::exceptions::SuccessExitException(options);
			}
		})
	    .addPositional(clah::FileParser::make("file", std::regex(".*\\.dmf$")))
	    .setHandler([](const clah::ParsingResult& options) -> int {
			if (auto threads = options.getValue<i64>("threads"); threads.has_value()) {
				if (threads.value() < 1)
					throw std::runtime_error("--threads must be a positive number");
				concurrent::worker::setWorkerCount(base::safeIntConv<usize>(threads.value()));
			} else {
				concurrent::worker::setWorkerCount(usize(std::thread::hardware_concurrency()));
			}
			query::setTrackReverseGraph(true);

			(void) options.getPositional<fs::File>(0);
			(void) query::entryPoint<ql::QueryCompilerVersion>(ql::VersionKey{});
			std::cerr << "querylinger: compilation pipeline not implemented yet\n";
			return 2;
		});
}

int main(int argc, const char** argv) {
	init::InitObject _;
	auto             clah = getQuerylingerClah();

	try {
		return clah.execute(base::safeIntConv<usize>(argc), argv);
	} catch (const std::exception& e) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception was caught with message:\n", printer::Color::Default },
			{ e.what(), printer::Color::Default },
			{ "\nAborting\n", printer::Color::Default },
		});
		return 1;
	} catch (...) {
		printer::StreamPrinter::print({
			{ "[ERROR] ", printer::Color::Red },
			{ "Unexpected Exception not inheriting from std::exception was caught.\n",
		      printer::Color::Default },
		});
		return 1;
	}
}
