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
	return clah::Clah("VM", "The Duckling Virtual Machine.")
	    .add(clah::ParamBuilder::ofFlag()
	             .addShortName('v')
	             .addLongName("version")
	             .addShortDesc("Shows version and config")
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
			pg::compile(options.getPositional<fs::File>(0));
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
