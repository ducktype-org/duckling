#pragma once

#include <frontend/pst_parser/elements/hierarchy/statements/import.hpp>
#include <frontend/pst_parser/pst.hpp>

#include <filesystem/file.hpp>
#include <string_id/string_id.hpp>

#include <memory>
#include <vector>

namespace pg {
	/// Parsed form of an import statement: `import a.b.c;`, `import a.b.c as X;` or
	/// `import a.b.*;`.
	struct ImportSpec {
		std::vector<base::StrID> path;      ///< Dotted path components (without the star).
		base::Optional<base::StrID> alias;  ///< `as` name, or the last path component; empty for `*`.
		bool star = false;
	};

	ImportSpec parseImportSpec(const pst::Import& import_stmt);

	/// One source file of the program. The root file has an empty module_path; an
	/// imported file lives in the nested namespace chain named by its path components
	/// (`import a.b.c` -> namespace a.b.c).
	struct LoadedModule {
		fs::File                    file;
		std::unique_ptr<pst::PST<>> pst;
		std::vector<base::StrID>    module_path;
	};

	/// Parses the root file and, transitively, every imported file. Import paths are
	/// always resolved relative to the root file's directory (`a.b.c` -> `a/b/c.dmf`),
	/// so the same file maps to the same module namespace no matter who imports it.
	/// Returns modules in dependency post-order (imported files before their importers,
	/// the root file last). Throws on missing files and cyclic imports.
	std::vector<LoadedModule> loadModules(const fs::File& root_file);
}
