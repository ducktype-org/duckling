#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Class Block that contains Class statements.
	 */
	class ClassBlock final: public NotStmt {
		std::vector<AccessInternalAnonymous<ClassStmt>> statements;

		base::Map<base::StrID, std::vector<AccessLocked<Stmt>>> by_symbol;
		std::vector<AccessLocked<Stmt>>                         no_symbol;
		std::vector<AccessLocked<Stmt>>                         transparent;

		void fillSymbols();

	public:
		DECLARE_CONST_ELEMENT_ITERATOR(statements, ClassStmt)

		explicit ClassBlock(const dia::SourcePosition& pos): NotStmt(pos) {
			this->element_kind = ElementKind::ClassBlock;
		}

		static MBox<ClassBlock> parse(LangParserState& state, const ClassContext& ctx);

		~ClassBlock() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Block";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}

		void calcElementPathsRecursive() override;
	};
}
