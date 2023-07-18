#pragma once

#include "symtable/symtable.hpp"
#include <pst_parser/pst.hpp>
#include <filesystem/file.hpp>
#include <vector>

#include "rift_symbols.hpp"

namespace hir {

	struct SourceUnit {
		pst::PST pst;
		fs::FilePath file_path;
	};

	// High intermediate representation
	class HIR {
		std::vector<SourceUnit> sources;
	public:
		// @TODO How to do imports?
		// @TODO Module system?, ignore for now
		void addUnit(SourceUnit&&);

		// @TODO: name
		void doMagicStuff();
	};
}
