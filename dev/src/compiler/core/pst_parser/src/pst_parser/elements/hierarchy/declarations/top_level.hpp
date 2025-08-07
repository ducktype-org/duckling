#pragma once

#include "preamble.hpp"

namespace pst {
	/**
	 * @brief Top-level element that is the root of the pst of a single file.
	 */
	class TopLevel final: public Decl {
		std::vector<AccessInternalAnonymous<Stmt>> statements;

	public:
		DECL_CHILD_CONSTRUCTOR(TopLevel, ElementKind::TopLevel);

		static MBox<TopLevel> parse(LangParserState& state);

		~TopLevel() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Top Level";
		}

		[[nodiscard]]
		auto getStatements() const {
			using namespace std::views;
			static auto give_one = [](auto& acc) -> AccessLocked<Stmt> { return acc.give(); };
			return std::ranges::ref_view(statements) | transform(give_one);
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;

		[[nodiscard]]
		
		
		DeclKind isDeclaration() const final {
			return DeclKind::Transparent;
		}

		/**
		 * @todo implement
		 */
		void calcElementPathsRecursive(const ElementPath&) override {
			CORE_PANIC("not implemented");
		}
	};
}
