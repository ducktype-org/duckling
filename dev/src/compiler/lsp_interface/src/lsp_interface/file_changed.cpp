#include "file_changed.hpp"

#include "frontend/pst_parser/lang_parser_element.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst_query/pst_access_side_input.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>

#include <query_framework/external/api.hpp>

namespace lsp {

	fs::File getFileFromVirtualRoot(const fs::File& virtual_root, const std::string& path) {
		if (virtual_root.getFilePath().join(path).exists())
			return fs::File(virtual_root.getFilePath().join(path));
		else {
			fs::FileManager::createVirtualFile(virtual_root.getFilePath().join(path), "");
			return fs::File(virtual_root.getFilePath().join(path));
		}
	}

	fs::File writeToFileFromVirtualRoot(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	) {
		auto file = getFileFromVirtualRoot(virtual_root, path);
		file.writeToFile(content);
		return file;
	}

	fs::File createFileFromVirtualRoot(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	) {
		auto file = getFileFromVirtualRoot(virtual_root, path);
		file.writeToFile(content);
		return file;
	}

	void collectQueryInputsFromPst(
		CRef<pst::PST<>> pst_ref, std::vector<query::external::InputData>& out
	) {
		auto root = pst_ref->getRootElement();
		if (auto maybe_root = root.illegalAccess()) {
			auto root_unlocked = maybe_root.value();
			out.emplace_back(pst::internal::PSTAccessSideInput::getID(), root_unlocked->getHash());

			auto elems = pst::viewAllSubTreeElements(root);
			for (auto& el: elems)
				if (auto maybe_elem = el.illegalAccess()) {
					auto ptr = maybe_elem.value();
					out.emplace_back(pst::internal::PSTAccessSideInput::getID(), ptr->getHash());
				}
		}
	}

	void debugPrintHashPaths(const std::vector<query::external::InputData>& inputs) {
		for (const auto& input: inputs) {
			std::cout << pst::LangElement::getByStableHash(input.hash).illegalAccess().value()->getElementPathHash().str() << " "
			<< input.hash << "\n";
		}
	}

	void updateFileContent(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	) {
		auto file = getFileFromVirtualRoot(virtual_root, path);

		auto source_files = compiler::frontend::SourceFile::getSourceFilesfromFile(file);

		std::vector<query::external::InputData> previous_inputs;
		// base::HashMap<pst::LangElement::HashType, std::string> hash_to_path;

		for (auto& source_file: source_files) {
			auto pst = source_file->getPST();
			collectQueryInputsFromPst(pst, previous_inputs);
		}

		// std::cout << "Previous inputs:\n";
		// debugPrintHashPaths(previous_inputs);

		// for (auto previous_input: previous_inputs) {
		// 	hash_to_path.put(previous_input.hash, pst::LangElement::getByStableHash(previous_input.hash).illegalAccess().value()->getElementPathHash().str());
		// }

		file = writeToFileFromVirtualRoot(virtual_root, path, content);
		compiler::frontend::ModuleTreeModifier::fileModified(file);

		std::vector<query::external::InputData> new_inputs;
		for (auto& source_file: source_files) {
			auto pst = source_file->getPST();
			collectQueryInputsFromPst(pst, new_inputs);
		}
		// std::cout << "New inputs:\n";
		// debugPrintHashPaths(new_inputs);

		
		std::vector<query::external::InputData> invalidated_inputs;
		query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {&invalidated_inputs});

		// std::cout << "Invalidated inputs:\n";
		// for (const auto& input: invalidated_inputs) {
		// 	if (hash_to_path.contains(input.hash)) {
		// 		std::cout << "Hash: " << input.hash << ", Path: " << hash_to_path[input.hash] << "\n";
		// 	} else {
		// 		std::cout << "Hash: " << input.hash << ", Path: (unknown)\n";
		// 	}
		// }
	}
}
