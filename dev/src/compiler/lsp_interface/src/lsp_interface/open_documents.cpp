#include <lsp_interface/open_documents.hpp>

namespace duck_ls {

	base::Ref<OpenDocument> OpenDocuments::insert(OpenDocument document) {
		auto path    = document.physical_path;
		auto [it, _] = documents.insert_or_assign(path, std::move(document));
		return &it->second;
	}

	bool OpenDocuments::erase(const fs::FilePath& path) { return documents.erase(path) > 0; }

	base::Optional<base::Ref<OpenDocument>> OpenDocuments::find(const fs::FilePath& path) {
		auto it = documents.find(path);
		if (it == documents.end()) return {};
		return base::Ref<OpenDocument>(&it->second);
	}

	base::Optional<base::CRef<OpenDocument>> OpenDocuments::find(const fs::FilePath& path) const {
		auto it = documents.find(path);
		if (it == documents.end()) return {};
		return base::CRef<OpenDocument>(&it->second);
	}

	bool OpenDocuments::contains(const fs::FilePath& path) const {
		return documents.contains(path);
	}

}
