#pragma once

#include <base/ints.hpp>
#include <base/perfect_hash.hpp>

namespace compiler::frontend {
    /**
	 * @brief Structure holding FileID within SourceFile
	 * @todo: change to STRONG_TYPEDEF_ID
	 */
	struct FileID final {
		[[nodiscard]]
		u64 asInt() const {
			return id;
		}

		static FileID nextID();
		bool          operator==(const FileID&) const = default;

		[[nodiscard]]
		base::HashT customPerfectHash() const {
			return asInt();
		}

	private:
		u64 id;
		FileID() = default;
	};
}

