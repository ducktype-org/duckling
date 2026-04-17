#pragma once

#include <diagnostic_interactive/stable_position.hpp>

#include <query_framework/external/api.hpp>
#include <diagnostic/source_position.hpp>

namespace query {
	struct Context;
}

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

		static query::external::InputData getQueryInputNode();
	};

	/**
	 * @brief This function ideally shouldn't be needed. It's for the cases when we are using a
	 * SourcePosition, but we obtained it in a way we don't have a query dependency on it, so we
	 * need to manually mark the dependency.
	 *
	 * @TODO: #2521 For example the tpc::Identifier has a SourcePosition, fix this
	 */
	void markDependencyOnSourcePosition(query::Context& ctx, dia::SourcePosition);

	/**
	 * @brief Helper class that can resolve to a SourcePosition, either from a StablePosition or
	 * from a custom resolver function. This is useful when we are writing a function that needs to
	 * report and error and has the position of the error as a parameter, but we only want to mark
	 * the dependency on the position when we actually need to report the error (the function may
	 * not always report an error).
	 */
	class ResolvesToPosition final {
		using ResolverFunction = std::function<dia::SourcePosition(query::Context&)>;
		std::variant<dia_int::StablePosition, ResolverFunction> data;

		static ResolverFunction fromSourcePosition(dia::SourcePosition position);

	public:
		ResolvesToPosition(dia_int::StablePosition pos): data(pos) {}

		ResolvesToPosition(dia::SourcePosition position): data(fromSourcePosition(position)) {}

		ResolvesToPosition(std::function<dia::SourcePosition(query::Context&)> resolver):
			  data(std::move(resolver)) {}

		[[nodiscard]] dia::SourcePosition resolve(query::Context& ctx) const;
	};
}
