#include <lsp_interface/files_cache.hpp>

#include <filesystem/file.hpp>

#include <algorithm>
#include <iostream>

namespace duck_ls {

	namespace {
		u32 utf16UnitsAt(std::string_view text, usize offset) {
			return static_cast<unsigned char>(text[offset]) >= 0xf0 ? 2 : 1;
		}

		usize codepointBytesAt(std::string_view text, usize offset) {
			const auto lead = static_cast<unsigned char>(text[offset]);
			if (lead >= 0xf0) return 4;
			if (lead >= 0xe0) return 3;
			if (lead >= 0xc0) return 2;
			return 1;
		}

		base::Optional<u32> toByteOffset(std::string_view text, const lsp::Position& position) {
			usize line_start = 0;
			for (u32 line = 0; line < position.line; ++line) {
				const auto newline = text.find('\n', line_start);
				if (newline == std::string_view::npos) return {};
				line_start = newline + 1;
			}

			const auto  newline  = text.find('\n', line_start);
			const usize line_end = newline == std::string_view::npos ? text.size() : newline + 1;

			usize offset    = line_start;
			u32   remaining = position.character;

			while (remaining > 0 && offset < line_end) {
				const auto units = utf16UnitsAt(text, offset);
				if (units > remaining) break;
				remaining -= units;
				offset += codepointBytesAt(text, offset);
			}

			if (remaining > 0) return {};
			return static_cast<u32>(offset);
		}

		bool applyChange(std::string& text, const lsp::TextDocumentContentChangeEvent& change) {
			if (const auto* whole
			    = std::get_if<lsp::TextDocumentContentChangeWholeDocument>(&change)) {
				text = whole->text;
				return true;
			}

			const auto* partial = std::get_if<lsp::TextDocumentContentChangePartial>(&change);
			if (partial == nullptr) return false;

			auto start = toByteOffset(text, partial->range.start);
			auto end   = toByteOffset(text, partial->range.end);
			if (start.empty() || end.empty() || start.value() > end.value()) return false;

			text.replace(start.value(), end.value() - start.value(), partial->text);
			return true;
		}

		bool isUnderRoot(const fs::FilePath& path, const fs::FilePath& root) {
			auto path_it  = path.getPath().begin();
			auto path_end = path.getPath().end();

			for (const auto& root_part: root.getPath()) {
				if (path_it == path_end) return false;
				if (*path_it != root_part) return false;
				++path_it;
			}

			return true;
		}
	}

	FilesCache::FilesCache(base::Optional<base::Ref<fs::VFS>> source_vfs): source_vfs(source_vfs) {}

	base::Optional<fs::FilePath> FilesCache::sourcePathFor(const lsp::Uri& uri) {
		if (!uri.isValid() || !uri.isFileUri()) return {};

		auto fs_path = uri.fsPath();
		if (fs_path.empty()) return {};

		fs::FilePath path = fs::FilePath(fs_path).lexicallyNormal();
		if (!path.isAbsolute()) return {};

		if (source_vfs.has_value()) return path.toVirtualPath(source_vfs.value());
		return path;
	}

	base::Optional<fs::FilePath> FilesCache::cachePathFor(const lsp::Uri& uri) {
		auto source = sourcePathFor(uri);
		if (source.empty()) return {};
		return cachePath(source.value());
	}

	fs::FilePath FilesCache::cachePath(const fs::FilePath& source_path) {
		if (source_path.isVirtual()) return source_path.withVfs(&cache_vfs);
		return source_path.toVirtualPath(&cache_vfs);
	}

	fs::FilePath FilesCache::sourcePath(const fs::FilePath& cache_path) {
		if (source_vfs.has_value()) return cache_path.withVfs(source_vfs.value());
		return cache_path.toPhysicalPath();
	}

	lsp::Uri FilesCache::uriFor(const fs::FilePath& path) {
		auto key = path.isVirtual() ? path.withVfs(&cache_vfs) : path;
		if_opt_some(find(key), document) return document->uri;

		auto physical = path.isVirtual() ? path.toPhysicalPath() : path;
		return lsp::Uri::parse(physical.uri());
	}

	bool FilesCache::isOpened(const fs::FilePath& source_file) {
		return cachePath(source_file).isRegularFile();
	}

	bool FilesCache::isOpened(const lsp::Uri& uri) {
		auto cache_path = cachePathFor(uri);
		if (cache_path.empty()) return false;
		return !find(cache_path.value()).empty();
	}

	void FilesCache::addWorkspaceRoot(const fs::FilePath& root) {
		if (std::ranges::find(workspace_roots, root) != workspace_roots.end()) return;
		workspace_roots.push_back(root);
	}

	base::Optional<fs::FilePath> FilesCache::workspaceRootFor(const fs::FilePath& path) {
		auto it = std::ranges::find_if(workspace_roots, [&](const fs::FilePath& root) {
			return isUnderRoot(path, root);
		});
		if (it == workspace_roots.end()) return {};
		return *it;
	}

	base::Optional<fs::FilePath> FilesCache::openDocument(
		const lsp::Uri& uri, std::string_view language_id, i32 version, std::string_view text
	) {
		auto source = sourcePathFor(uri);
		if (source.empty()) return {};

		auto cache_path = cachePath(source.value());
		fs::FileManager::createVirtualFile(cache_path, text, true);

		documents.insert_or_assign(
			cache_path,
			OpenDocument{
				.uri             = uri,
				.cache_path      = cache_path,
				.language_id     = std::string(language_id),
				.version         = version,
				.existed_on_disk = source.value().isRegularFile(),
			}
		);

		return cache_path;
	}

	base::OkBad FilesCache::updateDocument(
		const lsp::Uri&                                        uri,
		i32                                                    version,
		const lsp::Array<lsp::TextDocumentContentChangeEvent>& changes
	) {
		auto resolved = cachePathFor(uri);
		if (resolved.empty()) {
			std::cerr << "duck_ls: unsupported document URI: " << uri.toString() << "\n";
			return base::BAD;
		}

		const auto& cache_path = resolved.value();

		auto document = find(cache_path);
		if (document.empty()) {
			std::cerr << "duck_ls: change for a document that is not open: " << cache_path.strView()
					  << "\n";
			return base::BAD;
		}

		// Versions only ever increase; anything else means the buffers have diverged and
		// splicing at the client's offsets would silently corrupt the text.
		if (version <= document.value()->version) {
			std::cerr << "duck_ls: out of order version for " << cache_path.strView() << ": got "
					  << version << " after " << document.value()->version << ", ignoring\n";
			return base::BAD;
		}

		if (!cache_path.exists()) {
			std::cerr << "duck_ls: change for a document with no buffer: " << cache_path.strView()
					  << "\n";
			return base::BAD;
		}

		auto        file = fs::File(cache_path);
		std::string text{ file.getContent().view().stringView() };

		for (const auto& change: changes)
			if (!applyChange(text, change)) {
				std::cerr << "duck_ls: change out of range for " << cache_path.strView()
						  << ", the buffer is left as it was\n";
				return base::BAD;
			}

		file.writeToFile(text);
		document.value()->version = version;
		return base::OK;
	}

	base::OkBad FilesCache::closeDocument(const lsp::Uri& uri) {
		auto cache_path = cachePathFor(uri);
		if (cache_path.empty()) return base::BAD;

		if (cache_path.value().exists()) fs::FileManager::deleteFile(fs::File(cache_path.value()));
		return documents.erase(cache_path.value()) > 0 ? base::OK : base::BAD;
	}

	base::Optional<base::Ref<OpenDocument>> FilesCache::find(const fs::FilePath& cache_path) {
		auto it = documents.find(cache_path);
		if (it == documents.end()) return {};
		return base::Ref<OpenDocument>(&it->second);
	}

}
