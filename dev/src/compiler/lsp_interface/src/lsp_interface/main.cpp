#include <frontend/module_tree/module_flags/module_flags.hpp>
#include <lsp/io/stream.h>
#include <lsp_interface/compiler.hpp>
#include <lsp_interface/files_cache.hpp>
#include <lsp_interface/server_session.hpp>
#include <unistd.h>
#include <version/version.hpp>

#include <clah/clah.hpp>
#include <clah/clah_class.hpp>
#include <clah/param_builder.hpp>
#include <init/init.hpp>
#include <query_framework/module_flags/module_flags.hpp>

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <system_error>

namespace {

	/**
	 * @brief The protocol channel: reads from stdin, writes to a private handle on stdout.
	 *
	 * `lsp::io::standardIO()` writes through `stdout` itself, which is the one thing that must
	 * stop carrying the protocol, so the stream is built here instead.
	 */
	class ProtocolStream final: public lsp::io::Stream {
	public:
		explicit ProtocolStream(FILE* out): out(out) {}

		void read(char* buffer, std::size_t size) override {
			if (std::fread(buffer, size, 1, stdin) < 1 && std::ferror(stdin) != 0)
				throw lsp::io::Error(std::system_category().message(errno));
		}

		void write(const char* buffer, std::size_t size) override {
			if (std::fwrite(buffer, size, 1, out) < 1)
				throw lsp::io::Error(std::system_category().message(errno));
			std::fflush(out);
		}

	private:
		FILE* out;
	};

	/**
	 * @brief Takes stdout away from the rest of the program and points it at stderr.
	 *
	 * The protocol shares stdout with every `printf` and `std::cout` linked into this binary,
	 * and a single stray line corrupts the framing for the rest of the session. Claiming the
	 * descriptor before anything can write to it turns that whole class of bug into a log line.
	 *
	 * @return The handle the protocol writes to. On failure stdout is left exactly as it was and
	 * returned unchanged, so the server still runs with the original risk rather than not at all.
	 */
	FILE* claimStdout() {
		const int protocol_fd = dup(STDOUT_FILENO);
		if (protocol_fd < 0) return stdout;

		FILE* protocol_out = fdopen(protocol_fd, "wb");
		if (protocol_out == nullptr) {
			close(protocol_fd);
			return stdout;
		}

		std::fflush(stdout);

		if (dup2(STDERR_FILENO, STDOUT_FILENO) < 0) {
			std::fclose(protocol_out);
			return stdout;
		}

		return protocol_out;
	}

	/**
	 * @brief Serves one client over `stream` until it disconnects.
	 */
	int serve(lsp::io::Stream& stream) {
		lsp::ServerEndpoint endpoint(stream);

		duck_ls::ServerSession session{ base::Ref<lsp::ServerEndpoint>(&endpoint) };
		duck_ls::FilesCache    files;
		duck_ls::Compiler      compiler{ base::Ref<duck_ls::ServerSession>(&session),
                                    base::Ref<duck_ls::FilesCache>(&files) };

		session.registerHandlers(compiler);

		try {
			endpoint.runMessageLoop();
		} catch (const std::exception& e) {
			std::cerr << "duck_ls: " << e.what() << "\n";
			return 1;
		}

		return 0;
	}

	clah::Clah getDuckLsClah() {
		return clah::Clah("duck_ls", "The Duckling language server.")
		    .add(clah::ParamBuilder::ofFlag()
		             .addShortName('v')
		             .addLongName("version")
		             .addShortDesc("Print version and exit")
		             .build())
		    .add(clah::ParamBuilder::ofFlag()
		             .addLongName("version-verbose")
		             .addShortDesc("Print version together with build information and exit")
		             .build())
		    .add(clah::ParamBuilder::ofFlag()
		             .addLongName("stdio")
		             .addShortDesc("Serve on stdio. The only transport, accepted for the clients "
		                           "that pass it explicitly.")
		             .build())
		    .setPreHandler([](const clah::ParsingResult& options) {
				if (options.isFlag("version-verbose")) {
					std::cout << version::renderVerbose("duck_ls") << '\n';
					throw clah::exceptions::SuccessExitException(options);
				}
				if (options.isFlag("version")) {
					std::cout << version::renderShort("duck_ls") << '\n';
					throw clah::exceptions::SuccessExitException(options);
				}
			})
		    .setHandler([](const clah::ParsingResult&) -> int {
				query::setTrackReverseGraph(true);
				compiler::frontend::use_module_modifier_remove = true;

				ProtocolStream stream(claimStdout());
				return serve(stream);
			});
	}

}

int main(int argc, const char** argv) {
	init::InitObject _;

	auto clah = getDuckLsClah();
	return clah.execute(base::safeIntConv<usize>(argc), argv);
}
