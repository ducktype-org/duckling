#include <psackling/compiler.hpp>

#include <clah/clah.hpp>
#include <filesystem/file.hpp>
#include <init/init.hpp>
#include <logger/logger.hpp>
#include <printer/stream_printer.hpp>

#include <exception>
#include <regex>

void showVersion() { std::cout << "Psackling version -0.05\n"; }

clah::Clah getVmClah() {
	return clah::Clah("Psackling", "The PSAC-parallelized Duckling benchmark compiler.")
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Shows version and config")
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::IntParser::make())
	             .addShortName('j')
	             .addLongName("threads")
	             .addShortDesc("Number of PSAC worker threads (default: PARLAY_NUM_THREADS env "
	                           "or hardware concurrency).")
	             .build())
	    .add(clah::ParamBuilder::ofValue(clah::StringParser::make("path"))
	             .addShortName('o')
	             .addLongName("output")
	             .addShortDesc("Output executable path (default: psacklinged.out).")
	             .build())
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('s')
	             .addLongName("sequential")
	             .addShortDesc("Compile functions sequentially instead of through PSAC "
	                           "(baseline for benchmarks).")
	             .build())
	    .setPreHandler([](const clah::ParsingResult& options) {
			if (options.isFlag("version")) {
				showVersion();
				throw clah::exceptions::SuccessExitException(options);
			}
			if (options.isFlag("debug-logs")) {
				std::cerr << "Debug logs enabled.\n";
				logger::enable_dev_logs = true;
				logger::enableDevCategory(logger::DevLogCategories::Compiler);
				logger::enableDevCategory(logger::DevLogCategories::Parser);
				logger::setDevLogOutputStream(&std::cerr);
			}
		})
#ifdef BUILD_TYPE_DEV_DEBUG
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('d')
	             .addLongName("debug-logs")
	             .addShortDesc("Enables DVM debug logs.")
	             .build())
#endif
	    .addPositional(clah::FileParser::make("file", std::regex(".*\\.dmf$")))
	    .setHandler([](const clah::ParsingResult& options) -> int {
			pg::CompileOptions compile_options;
			if (auto output = options.getValue<std::string>("output"); output.has_value())
				compile_options.output_path = output.value();
			if (auto threads = options.getValue<i64>("threads"); threads.has_value()) {
				if (threads.value() < 1)
					throw std::runtime_error("--threads must be a positive number");
				compile_options.num_threads = base::safeIntConv<usize>(threads.value());
			}
			compile_options.sequential = options.isFlag("sequential");
			pg::compile(options.getPositional<fs::File>(0), compile_options);
			return 0;
		});
}

int main(int argc, const char** argv) {
	init::InitObject _;
	auto             clah = getVmClah();

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
