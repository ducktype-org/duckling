#pragma once

#include <lsp/io/stream.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace duck_ls_test {

	/**
	 * @brief An in-memory `lsp::io::Stream` feeding a fixed script to the server and collecting
	 * everything it writes back.
	 *
	 * Reaching the end of the script reports EOF rather than throwing, because the framework
	 * peeks a single byte to detect a closed connection.
	 */
	class StringStream final: public lsp::io::Stream {
	public:
		explicit StringStream(std::string input): input(std::move(input)) {}

		void read(char* buffer, std::size_t size) override {
			if (size == 0) return;

			if (read_position >= input.size()) {
				if (size == 1) {
					*buffer = Eof;
					return;
				}
				throw lsp::io::Error("StringStream: unexpected end of input");
			}

			const auto available = std::min(size, input.size() - read_position);
			std::memcpy(buffer, input.data() + read_position, available);
			read_position += available;

			if (available < size) throw lsp::io::Error("StringStream: unexpected end of input");
		}

		void write(const char* buffer, std::size_t size) override { output.append(buffer, size); }

		[[nodiscard]] const std::string& written() const { return output; }

	private:
		std::string input;
		std::string output;
		std::size_t read_position = 0;
	};

	/**
	 * @brief Wraps a JSON-RPC body in the header framing the protocol expects.
	 */
	inline std::string frame(std::string_view body) {
		return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
	}

}
