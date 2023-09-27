#include <iomanip>

#include "cli.hpp"
#include "server.hpp"
#include <clap/clap.hpp>
#include <supervisor/supervisor.hpp>

#include <services/executor_f8/op_case_config.hpp>

void initialise([[maybe_unused]] const clap::ParametersMap &params) {
	// @TODO
}

void showVersion() {
	std::cout << std::boolalpha;
	std::cout << "RiftVM version 0.0.\n";
	std::cout << "Configuration: \n";
	std::cout << "IGNORE_EXECUTION_STRATEGY: " << IGNORE_EXECUTION_STRATEGY << "\n";
	std::cout << "USE_COMPUTED_GOTO: " << USE_COMPUTED_GOTO_VALUE << "\n";
	std::cout << "USE_FLAT_FRAME: " << USE_FLAT_FRAME_VALUE << "\n";
}

int main(int argc, char **argv) {
	clap::ParametersMap parameters
		= clap::Config()
	          .add(clap::ParameterConfig("server")
	                   .short_name('s')
	                   .description("Launch RiftVM as a http server")
	                   .with_value("port", "5000"))
	          .add(clap::ParameterConfig("file")
	                   .short_name('f')
	                   .description("Launch given file (only if not -serwer)")
	                   .with_value("filename", "<@FIXME>"))
	          .add(clap::ParameterConfig("version").short_name('v').description(
				  "Shows version and config"
			  ))
	          .parse(clap::CLIArgs{ argc, argv });

	initialise(parameters);
	// Instantiate supervisor
	vm::Supervisor::get();

	if (parameters.contains('v')) {
		showVersion();
	} else if (parameters.contains('s')) {
		server(std::stoi(parameters.get('s').value().stdString()));
	} else if (parameters.contains('f')) {
		auto file = parameters.get('f').value().stdString();
		cli(file);
	} else {
		cli();
	}
}
