#pragma once

#include "../meta.hpp"

namespace pst {
	/**
	 * @brief Class Block that contains Class statements.
	 */
	class ClassBlock final: public NotStmt {
		std::vector<AccessInternalAnonymous<ClassStmt>> statements;

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

		/**
		 * @todo implement
		 */
		void calcElementPathsRecursive(const ElementPath&) override {
			CORE_PANIC("not implemented");
		}
	};
}
