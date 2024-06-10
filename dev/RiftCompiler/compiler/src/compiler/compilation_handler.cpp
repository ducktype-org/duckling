#include "compilation_handler.hpp"
#include <pst_parser/parser.hpp>
#include <module_system/import_to_path.hpp>

namespace compiler {
	void CompilationHandler::addFileRecursively(
		const fs::FilePath& path, bool dprint, std::ostream* out
	) {
		RIFT_ASSERT(!dprint or out != nullptr, "out cannot by nullptr when dprint is true");
		std::ostream& out_ref = *out;

		if (pst_map.contains(path)) return;

		if (dprint) out_ref << "Parsing new file: `" << path.strView() << "`\n";

		pst_map.put(path, pst::PST(path));
		pst::PST<>& new_pst = pst_map[path];

		if (new_pst.getLogger().bad()) {
			if (dprint) {
				new_pst.getLogger().dumpLog(false, out_ref);
				out_ref << "\nThere were errors while parsing file: " << path.strView() << "\n";
			}
			return;
		}

		auto& imports = new_pst.getImports();

		if (dprint) {
			if (imports.empty()) {
				out_ref << "  No imports. \n";
			} else {
				out_ref << "  Imports: \n";
				for (auto& import: imports) {
					out_ref << "   "
							<< modulesys::importToPath(path.parentPath().strView(), *import)
							<< "\n";
				}
				out_ref << "\n";
			}
		}

		// @TODO: we should handle different import syntax like .* or .function_name
		for (auto& import: imports) {
			auto import_path = modulesys::importToPath(path.parentPath().strView(), *import);
			addFileRecursively(fs::FilePath(import_path), dprint, out);
		}
	}

}
