#include <concurrent/module_flags/worker_count.hpp>
#include <querylinger/session.hpp>

#include <clah/clah.hpp>
#include <filesystem/file.hpp>
#include <init/init.hpp>
#include <logger/logger.hpp>
#include <printer/stream_printer.hpp>

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
			ql::CompileOptions compile_options;
			if (auto output = options.getValue<std::string>("output"); output.has_value())
				compile_options.output_path = output.value();
			if (auto threads = options.getValue<i64>("threads"); threads.has_value()) {
				if (threads.value() < 1)
					throw std::runtime_error("--threads must be a positive number");
				compile_options.num_threads = base::safeIntConv<usize>(threads.value());
			} else {
				compile_options.num_threads = usize(std::thread::hardware_concurrency());
			}
			concurrent::worker::setWorkerCount(compile_options.num_threads);

			if (!options.isFlag("interactive")) {
				ql::compile(options.getPositional<fs::File>(0), compile_options);
				return 0;
			}

			// Interactive mode: keep the query graph alive and feed it new source.
		    // Each stdin line is a path to recompile incrementally (empty line repeats
		    // the previous file, e.g. after editing it in place). Same protocol as
		    // psacklinger's interactive mode.
			ql::Compiler compiler(compile_options);

			auto report = [](const ql::CompileStats& stats) {
				std::cout << "recompiled " << stats.recompiled_functions << "/"
						  << stats.total_functions << " functions\n"
						  << std::flush;
			};

			std::filesystem::path current_path = options.getPositional<fs::File>(0).getFilePath();
			report(compiler.compile(options.getPositional<fs::File>(0)));

			std::string line;
			while (std::getline(std::cin, line)) {
				if (line == "quit" || line == "exit") break;
				if (!line.empty()) current_path = line;
				if (!std::filesystem::exists(current_path)) {
					std::cout << "no such file: " << current_path.string() << "\n" << std::flush;
					continue;
				}
				try {
					report(compiler.update(fs::File(current_path)));
				} catch (const std::exception& e) {
					std::cout << "compilation failed: " << e.what() << "\n" << std::flush;
				}
			}
			return 0;
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
