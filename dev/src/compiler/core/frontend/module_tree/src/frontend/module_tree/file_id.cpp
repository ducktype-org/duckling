#include <base/config/build_type.hpp>

#include "file_id.hpp"
#ifdef BUILD_TYPE_DEV
	#include "source_file.hpp"
#endif

namespace compiler::frontend {
	void FileID::checkDanglingReference() const {
		IF_BUILD_TYPE_DEV(SourceFile::checkDanglingReference(ref));
	}
}
