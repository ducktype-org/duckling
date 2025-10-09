#pragma once

#include "../meta.hpp"

#include <base/optional.hpp>

namespace pst {
	/**
	 * @brief Call argument, handles both named and normal arguments.
	 */
	class CallArgument final: public NotStmt {
		tpc::OptionalIdentifier arg_name;
		NAMED_CHILD(arg, UniversalExprHolderLowerLevel);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

	public:
		explicit CallArgument(const dia::SourcePosition& pos): NotStmt(pos) {
			this->element_kind = ElementKind::CallArgument;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Call argument";
		}

		[[nodiscard]]
		bool isNamedArg() const {
			return arg_name.value.has_value();
		}

		[[nodiscard]]
		tpc::OptionalIdentifier getArgName() const {
			return arg_name;
		}

		[[nodiscard]]
		AccessLocked<UniversalExprHolderLowerLevel> getArg() const {
			return arg.give();
		}

		static MBox<CallArgument> parse(LangParserState& state);

		~CallArgument() final = default;
		void dprint(std::ostream& out) const final;
	};
}
