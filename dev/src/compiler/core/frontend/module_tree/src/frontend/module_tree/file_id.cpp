// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "file_id.hpp"

#include "source_file.hpp"

#include <base/config/build_type.hpp>

namespace compiler::frontend {
	FileID::FileID(base::Ref<SourceFile> ref): hash(ref->getComponentHash().hash) {}

	base::Ref<SourceFile> FileID::resolve() const {
		auto ref = SourceFile::getRegisteredFile(hash);
		IF_BUILD_TYPE_DEV(SourceFile::checkDanglingReference(ref));
		return ref;
	}
}
