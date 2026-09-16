#include <lsp_interface/uri_conversion.hpp>

namespace duck_ls {

	base::Optional<fs::FilePath> toPhysicalPath(const lsp::Uri& uri) {
		if (!uri.isValid() || !uri.isFileUri()) return {};

		auto fs_path = uri.fsPath();
		if (fs_path.empty()) return {};

		fs::FilePath path(fs_path);
		if (!path.isAbsolute()) return {};

		return path.lexicallyNormal();
	}

	lsp::Uri toDocumentUri(const fs::FilePath& path, const OpenDocuments& documents) {
		if (path.isVirtual()) {
			auto physical = path.toPhysicalPath();
			if_opt_some(documents.find(physical), document) return document->uri;
			return lsp::Uri::fileUriFromPath(physical.genericString());
		}

		if_opt_some(documents.find(path), document) return document->uri;
		return lsp::Uri::fileUriFromPath(path.genericString());
	}

}
