#include "module_loader.hpp"

#include <frontend/pst_parser/elements/hierarchy/declarations/top_level.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/import_chains/import_identifier_as.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/import_chains/import_star_hides.hpp>

#include <base/collections/maps.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace {
	std::string dottedPath(const std::vector<base::StrID>& path) {
		std::string out;
		for (base::StrID component: path) {
			if (!out.empty()) out += '.';
			out += component.str();
		}
		return out;
	}

	struct Loader {
		fs::FilePath root_dir;

		std::vector<pg::LoadedModule> modules{};
		/// Canonical path -> fully loaded. `false` means the file is still on the DFS
		/// stack, so seeing it again is an import cycle.
		base::HashMap<std::string, bool> visited{};

		fs::File resolveImportFile(const std::vector<base::StrID>& path) {
			// Join below the FilePath level: FilePath::join classifies bare relative
			// segments by CWD (e.g. Temporary under /tmp) and then refuses to join them.
			std::filesystem::path resolved = root_dir.getPath();
			for (base::StrID component: path) resolved /= component.str();
			resolved += ".dmf";

			fs::File file(fs::FilePath{ resolved });
			if (!file.exists()) {
				throw std::runtime_error(
					"import " + dottedPath(path) + ": file not found: " + file.getFilePath().string()
				);
			}
			return file;
		}

		void load(fs::File file, std::vector<base::StrID> module_path) {
			const std::string key = file.getFilePath().string();
			if (visited.contains(key)) {
				if (!visited[key]) throw std::runtime_error("import cycle detected through " + key);
				return;
			}
			visited.insertOrAssign(key, false);

			// Skip the PST's SHA-256 stable hashes — incremental diffing uses the
			// compiler's own dprint-based content hashes instead.
			auto pst = std::make_unique<pst::PST<>>(
				file, pst::PSTType::Program, hashing::ComponentHash{}, pst::PSTHashing::Skip
			);

			for (pst::AccessLocked<pst::Stmt> stmt_locked:
			     pst->getRootElement().illegalAccess().value()->getStatements()) {
				pst::Access<pst::Stmt> stmt = stmt_locked.illegalAccess().value();

				auto import_opt = stmt.dynamicCast<pst::Import>();
				if (!import_opt.has_value()) continue;

				pg::ImportSpec spec = pg::parseImportSpec(*import_opt.value());
				load(resolveImportFile(spec.path), spec.path);
			}

			visited[key] = true;
			modules.push_back(pg::LoadedModule{ .file        = std::move(file),
			                                    .pst         = std::move(pst),
			                                    .module_path = std::move(module_path) });
		}
	};
}

namespace pg {
	ImportSpec parseImportSpec(const pst::Import& import_stmt) {
		pst::Access<pst::ImportChain> chain = import_stmt.getImportChain().illegalAccess().value();

		ImportSpec spec;

		auto read_names = [&](const auto& chain_impl) {
			for (usize i = 0; i < chain_impl->numberOfNames(); ++i)
				spec.path.push_back(chain_impl->getNameByIndex(i).illegalAccess().value()->unwrap());
		};

		if (auto as_chain = chain.dynamicCast<pst::ImportIdentifierAs>(); as_chain.has_value()) {
			read_names(as_chain.value());
			if (auto as = as_chain.value()->asWhat(); as.has_value())
				spec.alias = as.value().illegalAccess().value()->unwrap();
			else
				spec.alias = spec.path.back();
		} else if (auto star_chain = chain.dynamicCast<pst::ImportStarHides>();
		           star_chain.has_value()) {
			if (star_chain.value()->isImportHides())
				throw std::runtime_error("import: 'hides' is not supported");
			read_names(star_chain.value());
			spec.star = true;
		} else {
			throw std::runtime_error(
				"import: chain kind '" + chain->elementType() + "' is not supported"
			);
		}

		if (spec.path.empty()) throw std::runtime_error("import: empty import path");
		return spec;
	}

	std::vector<LoadedModule> loadModules(const fs::File& root_file) {
		Loader loader{ .root_dir = root_file.getFilePath().parentPath() };
		loader.load(root_file, {});
		return std::move(loader.modules);
	}
}
