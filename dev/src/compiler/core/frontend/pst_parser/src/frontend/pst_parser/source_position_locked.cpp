
#include "source_position_locked.hpp"

#include <query_framework/input_query/query_input.hpp>
#include <query_framework/input_query/query_input_impl.hpp>

namespace pst {
	struct FileSourceCode {
		[[nodiscard]]
		query::QueryStableHash queryStablePerfectHash() const {
			// 4 random numbers
			return { 62'680'354, 72'959'470, 8'833'575, 82'097'363 };
		}
	};

	/**
	 * @brief This is a placeholder for future input query,
	 * that will track the hash of the source code of the module, so when the source code changes,
	 * the debug info positions will be recalculated.
	 * @TODO: #2329 Change this.
	 *
	 * @note It will always be invalidated, which is for now what we want.
	 */
	DECLARE_QUERY_SIDE_INPUT(FileSourcePositions, FileSourceCode);
	IMPLEMENT_QUERY_SIDE_INPUT(FileSourcePositions);

	dia::SourcePosition SourcePositionLocked::unlock(query::Context& ctx) const {
		ctx.query<FileSourcePositions>(FileSourceCode{});
        return source_position;
	}

	dia::SourcePosition SourcePositionLocked::illegalAccess() const {
        return source_position;
    }
}
