#pragma once

#include "../../lang_parser_state.hpp"

#include <diagnostic/source_position.hpp>

#include <token_parser_core/token_stream.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/automatic.hpp>

#include <base/string_id.hpp>

#include "meta.hpp"
#include "not_statements.hpp"

namespace pst {
	/**
	 * @note: Import allows for two syntaxes right now:
	 * import A.B as D;
	 * import A.B.* as D;
	 *
	 * the optional "star" is ignored.
	 */
	class Import final: public Stmt {
		MBox<DottedName> names;
		tpc::Identifier  alias;

	public:
		STMT_CHILD_CONSTRUCTOR(Import, ElementKind::Import);
		static MBox<Import> parse(LangParserState& state);
		[[nodiscard]]
		const decltype(names)& getNames() const;

		[[nodiscard]]
		base::StrID getAlias() const {
			return alias.value;
		}

		/**
		 * @note In the future this functionality will be done by HELIOS.
		 * This functionality is needed to implement early import system for testing.
		 */
		[[nodiscard]]
		std::vector<base::StrID> getModulePath() const;

		[[nodiscard]]
		bool getStar() const;
		~Import() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Import";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};

	class Using final: public Stmt {
		MBox<DottedName> names;

	public:
		STMT_CHILD_CONSTRUCTOR(Using, ElementKind::Using);
		static MBox<Using> parse(LangParserState& state);

		[[nodiscard]]
		auto getPointed() const {
			return names->getNames();
		}

		[[nodiscard]]
		bool isStar() const {
			return names->getStar();
		}

		~Using() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Using";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};

	class ExprStmt final: public Stmt {
		MBox<ExprElement> expr;

	public:
		explicit ExprStmt(dia::SourcePosition pos): Stmt(StmtKind::ExprStmt, pos) {
			this->element_kind = ElementKind::ExprStmt;
		}

		static MBox<ExprStmt> parse(LangParserState& state);

		~ExprStmt() override = default;
		void dprint(std::ostream& out) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Expr Stmt";
		}

		[[nodiscard]]
		MCRef<ExprElement> getExpr() const {
			return expr.ref();
		}

		void acceptVisitor(PstStmtVisitor&) const override;
	};

	class Alias final: public Stmt {
		tpc::Identifier  name;
		MBox<DottedName> points_to;

	public:
		STMT_CHILD_CONSTRUCTOR(Alias, ElementKind::Alias);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		auto getPointed() const {
			return points_to->getNames();
		}

		static MBox<Alias> parse(LangParserState& state);
		~Alias() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Alias";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};

	/**
	 * @note Action assumes optional expression before the semicolon.
	 */
	class Action: public Stmt {
	protected:
		base::Optional<MBox<CommaExprHolder>> expr;

	public:
		STMT_CHILD_CONSTRUCTOR(Action, ElementKind::Action);
		static MBox<Action> parse(LangParserState& state);
		~Action() override = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Action";
		}

		[[nodiscard]]
		/**
		 * @note Optional of MRef here is intentional
		 */
		base::Optional<MCRef<ExprHolder>> getValue() const {
			return expr.map([](const auto& e) -> MCRef<ExprHolder> { return e.ref(); });
		}
	};

	// TODO: Merge it with variable. Or perhaps make a new class DataStorage.
	class Const final: public Stmt {
		tpc::Identifier       name;
		MBox<CommaExprHolder> type;
		MBox<CommaExprHolder> value;

	public:
		STMT_CHILD_CONSTRUCTOR(Const, ElementKind::Const);
		static MBox<Const> parse(LangParserState& state);

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		MCRef<ExprHolder> getType() const {
			return type.ref();
		}

		[[nodiscard]]
		MCRef<ExprHolder> getValue() const {
			return value.ref();
		}

		~Const() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Const";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return true;
		}
	};

	/**
	 * @todo should assert be an action
	 */
	class Decl: public Stmt {
	public:
		Decl(StmtKind kind, const dia::SourcePosition& position): Stmt(kind, position) {}

		bool trailingSemicolon() override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Declaration";
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}
	};

	/**
	 * @brief Class designated to be the parent of non-functional statements that are only used
	 * internally for testing.
	 */
	class TestingStmt: public Stmt {
	public:
		TestingStmt(StmtKind kind, const dia::SourcePosition& position): Stmt(kind, position) {}
	};

#define TEST_CHILD_CONSTRUCTOR(class_name) \
	class_name(const dia::SourcePosition& position): TestingStmt(StmtKind::class_name, position) {}

}
