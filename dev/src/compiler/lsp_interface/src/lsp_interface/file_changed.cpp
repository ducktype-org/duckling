#include "file_changed.hpp"

#include <frontend/module_tree/module_tree.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/pst_query/pst_access_side_input.hpp>
#include <frontend/pst_parser/test_utils/pst_test_utils.hpp>

#include <query_framework/external/api.hpp>

namespace lsp {
	using fs::File;

	namespace {
		/**
		 * @brief Helper to get file from virtual root, creating it if it doesn't exist.
		 */
		fs::File getFileFromVirtualRoot(const fs::File& virtual_root, const std::string& path) {
			if (virtual_root.getFilePath().join(path).exists())
				return { virtual_root.getFilePath().join(path) };
			else {
				fs::FileManager::createVirtualFile(virtual_root.getFilePath().join(path), "");
				return { virtual_root.getFilePath().join(path) };
			}
		}

		fs::File writeToFileFromVirtualRoot(
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
				out.emplace_back(
					pst::internal::PSTAccessSideInput::getID(), root_unlocked->getHash()
				);

				auto elems = pst::viewAllSubTreeElements(root);
				for (auto& el: elems)
					if (auto maybe_elem = el.illegalAccess()) {
						auto ptr = maybe_elem.value();
						out.emplace_back(pst::internal::PSTAccessSideInput::getID(), ptr->getHash());
					}
			}
		}
	}

	fs::File createFileFromVirtualRoot(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	) {
		auto file = getFileFromVirtualRoot(virtual_root, path);
		file.writeToFile(content);
		return file;
	}

	void updateFileContent(
		const fs::File& virtual_root, const std::string& path, const std::string& content
	) {
		auto file = getFileFromVirtualRoot(virtual_root, path);

		auto source_files = compiler::frontend::SourceFile::getSourceFilesFromFile(file);

		std::vector<query::external::InputData> previous_inputs;

		for (auto& source_file: source_files) {
			auto pst = source_file->getPST();
			collectQueryInputsFromPst(pst, previous_inputs);
		}

		file = writeToFileFromVirtualRoot(virtual_root, path, content);
		compiler::frontend::ModuleTreeModifier::fileModified(file);

		std::vector<query::external::InputData> new_inputs;
		for (auto& source_file: source_files) {
			auto pst = source_file->getPST();
			collectQueryInputsFromPst(pst, new_inputs);
		}

		// std::vector<query::external::InputData> invalidated_inputs;
		query::external::invalidateQueries(std::move(new_inputs), { previous_inputs }, {});
	}
}
