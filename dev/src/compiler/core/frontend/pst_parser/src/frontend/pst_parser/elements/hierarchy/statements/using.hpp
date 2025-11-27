#pragma once

#include "../meta.hpp"

#include <frontend/pst_parser/elements/hierarchy/not_statements/dotted_name.hpp>

namespace pst {
	/**
	 * @brief Using statement
	 */
	class Using final: public Stmt {
		NAMED_CHILD(names, DottedName);

	public:
		STMT_CHILD_CONSTRUCTOR(Using, ElementKind::Using);
		static MBox<Using> parse(LangParserState& state);

		[[nodiscard]]
		auto getPointed() const {
			return names.give();
		}

		~Using() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Using";
		}

		[[nodiscard]]
		DeclKind isDeclaration() const final {
			return DeclKind::Symbol;
		}

		[[nodiscard]] base::Optional<base::StrID> getDeclSymbolName() const final;
	};
}
