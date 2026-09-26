#include "verify.hpp"

#include <sys/wait.h>

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace c_import {

	namespace {

		/** Single-quotes a path for the shell; an embedded quote is escaped the POSIX way. */
		std::string shellQuoted(const std::string& text) {
			std::string out = "'";
			for (const char character: text)
				if (character == '\'')
					out += "'\\''";
				else
					out.push_back(character);
			out.push_back('\'');
			return out;
		}

		std::string readAll(const std::filesystem::path& path) {
			std::ifstream file(path, std::ios::binary);
			if (!file) return {};
			std::ostringstream contents;
			contents << file.rdbuf();
			return contents.str();
		}

		/** Keeps the tail of the compiler output: the errors are at the end. */
		std::string lastLines(const std::string& text, std::size_t limit) {
			std::vector<std::string> lines;
			std::istringstream       stream(text);
			for (std::string line; std::getline(stream, line);) lines.push_back(line);

			const std::size_t first = lines.size() > limit ? lines.size() - limit : 0;

			std::ostringstream out;
			if (first != 0) out << "... (" << first << " earlier lines omitted)\n";
			for (std::size_t i = first; i < lines.size(); i++) out << lines[i] << '\n';
			return out.str();
		}

	}

	VerifyResult verifyPackage(
		const std::filesystem::path& out_dir,
		const std::string&           package_name,
		const std::string&           duckc
	) {
		std::error_code error_code;

		// A dot-prefixed directory is skipped by the module tree scan, so leaving it behind
		// cannot turn into a stray module.
		const std::filesystem::path artifacts = out_dir / ".duck_verify";
		std::filesystem::create_directories(artifacts, error_code);

		const std::filesystem::path log = artifacts / "compile.log";

		std::ostringstream command;
		command << shellQuoted(duckc) << " compile_package "
				<< shellQuoted((out_dir / "src").string()) << " -a "
				<< shellQuoted(artifacts.string()) << " -n " << shellQuoted(package_name)
				<< " --emit-static-lib";

		VerifyResult result;
		result.command = command.str();

		const std::string redirected = command.str() + " > " + shellQuoted(log.string()) + " 2>&1";

		// The tool is single threaded and the compiler has to run as a subprocess.
		const int status = std::system(redirected.c_str());  // NOLINT(concurrency-mt-unsafe)

		result.output = lastLines(readAll(log), 40);
		result.ok     = status == 0;

		// The shell reports a command it could not find as 127, which says nothing about the
		// package: it was written fine and is perfectly usable.
		result.compiler_missing = status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 127;

		std::filesystem::remove_all(artifacts, error_code);

		return result;
	}

}
