#pragma once

#include <diagnostic/source_position.hpp>
#include <query_framework/context/context.hpp>

namespace pst {
	class LangElement;

	class SourcePositionLocked {
		dia::SourcePosition source_position;

		SourcePositionLocked(dia::SourcePosition source_position):
			  source_position(source_position) {}

	public:
		[[nodiscard]] dia::SourcePosition unlock(query::Context& ctx) const;
		[[nodiscard]] dia::SourcePosition illegalAccess() const;

		friend class LangElement;
	};
}
