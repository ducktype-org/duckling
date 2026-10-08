// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include <lsp/io/stream.h>
#include <lsp/types.h>
#include <lsp/uri.h>
#include <lsp_interface/server_session.hpp>

#include <base/pointers/ref.hpp>

#include <filesystem/file.hpp>
#include <filesystem/file_path.hpp>
#include <filesystem/vfs.hpp>

#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
		std::string input{};
		std::string output{};
		std::size_t read_position = 0;
	};

	/**
	 * @brief Wraps a JSON-RPC body in the header framing the protocol expects.
	 */
	inline std::string frame(std::string_view body) {
		return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + std::string(body);
	}

	/**
	 * @brief Builds a source tree in the singleton VFS, standing in for the hard drive.
	 */
	class VfsWorkspace final {
	public:
		explicit VfsWorkspace(std::string_view name):
			  root_path(fs::FilePath(vfs()->getRootPath()).join(std::string(name))) {
			fs::FileManager::createVirtualFolder(root_path, true);
		}

		/**
		 * @brief Creates a file at `relative` below the root, with every directory above it.
		 */
		VfsWorkspace& add(std::string_view relative, std::string_view content) {
			fs::FileManager::createVirtualFile(root_path.join(std::string(relative)), content, true);
			return *this;
		}

		VfsWorkspace& remove(std::string_view relative) {
			fs::FileManager::deleteFile(fs::File(root_path.join(std::string(relative))));
			return *this;
		}

		[[nodiscard]] lsp::Uri uriOf(std::string_view relative = "") const {
			const auto path = relative.empty() ? root_path : root_path.join(std::string(relative));
			return lsp::Uri::fileUriFromPath(path.toPhysicalPath().genericString());
		}

		[[nodiscard]] fs::FilePath pathOf(std::string_view relative) const {
			return root_path.join(std::string(relative));
		}

		[[nodiscard]] static base::Ref<fs::VFS> vfs() { return fs::VFS::getInstance(); }

	private:
		fs::FilePath root_path;
	};

	/**
	 * @brief A session that keeps what it was asked to push instead of writing to a client.
	 *
	 * The endpoint it is built on is never used: `pushDiagnostics` is the only thing the
	 * compiler calls, and it is overridden here.
	 */
	struct CollectingSession final: public duck_ls::ServerSession {
		using ServerSession::ServerSession;

		std::unordered_map<lsp::Uri, std::vector<lsp::Diagnostic>> pushed;

		void pushDiagnostics(const lsp::Uri& uri, const std::vector<lsp::Diagnostic>& diagnostics)
			override {
			pushed[uri] = diagnostics;
		}

		/**
		 * @brief Whether the last publish for `uri` reported something.
		 */
		[[nodiscard]] bool hasErrors(const lsp::Uri& uri) const {
			auto it = pushed.find(uri);
			return it != pushed.end() && !it->second.empty();
		}

		/**
		 * @brief Whether `uri` was published with nothing to report.
		 *
		 * False for a URI that was never published at all: a file the server said nothing about
		 * is not a file the server found clean.
		 */
		[[nodiscard]] bool noErrors(const lsp::Uri& uri) const {
			auto it = pushed.find(uri);
			return it != pushed.end() && it->second.empty();
		}
	};
}
