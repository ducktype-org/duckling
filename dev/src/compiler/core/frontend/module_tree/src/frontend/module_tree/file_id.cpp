#include "file_id.hpp"

#include <base/config/build_type.hpp>
#include "source_file.hpp"

namespace compiler::frontend {
	void FileID::checkDanglingReference() const {
		IF_BUILD_TYPE_DEV(SourceFile::checkDanglingReference(ref));
	}
}
