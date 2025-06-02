#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Using statement
	 */
	class Using final: public Stmt {
		AccessInternal<DottedName> names;

	public:
		STMT_CHILD_CONSTRUCTOR(Using, ElementKind::Using);
		static MBox<Using> parse(LangParserState& state);

		[[nodiscard]]
		auto getPointed() const {
			return names.give();
		}

		[[nodiscard]]
		bool isStar() const;

		~Using() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Using";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};
}
