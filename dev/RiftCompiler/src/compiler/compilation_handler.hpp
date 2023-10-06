#pragma once

#include <base/maps.hpp>
#include <filesystem/file.hpp>
#include <hir/symtable/symtable.hpp>
#include <pst_parser/pst.hpp>

#include <ostream>

namespace compiler {

	// @TODO
	// This class is currently a copy of deleted ParsingHandler
	// It should perform only top-level operations, and
	// details should be moved elsewhere to this module or other
	class CompilationHandler {
		base::HashMap<fs::FilePath, pst::PST> pst_map;

	public:
		CompilationHandler() = default;

		void addFileRecursively(
			const fs::FilePath& path, bool dprint = false, std::ostream* out = nullptr
		);

		usize pstCount() const { return pst_map.size(); }
	};
}
