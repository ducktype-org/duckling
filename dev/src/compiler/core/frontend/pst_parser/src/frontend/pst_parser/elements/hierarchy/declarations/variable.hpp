#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Variable declaration
	 */
	class Variable final: public Decl {
		tpc::Identifier name;
		NAMED_CHILD_OPT(type, CommaExprHolder);
		NAMED_CHILD_OPT(value, CommaExprHolder);
		bool is_const = true;

		template<typename T, lang_def::Keyword key>
		friend MBox<T> parseVariableTemplate(pst::LangParserState& state);

	protected:
		HashAlg& addElementDataToStableHash(HashAlg& partial_hash) const override;

	public:
		DECL_CHILD_CONSTRUCTOR(Variable, ElementKind::Variable);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		tpc::Identifier getNameIdent() const {
			return name;
		}

		bool trailingSemicolon() override;

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getType() const {
			if (type.has_value()) return { type.value().give() };
			return {};
		}

		[[nodiscard]]
		base::Optional<AccessLocked<ExprHolder>> getValue() const {
			if (value.has_value()) return { value.value().give() };
			return {};
		}

		[[nodiscard]]
		bool isConst() const {
			return is_const;
		}

		[[nodiscard]]
		base::Optional<base::StrID> getDeclSymbolName() const final {
			return getName();
		}

		static MBox<Variable> parse(LangParserState& state);
		~Variable() final = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return is_const ? "Let" : "Var";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
