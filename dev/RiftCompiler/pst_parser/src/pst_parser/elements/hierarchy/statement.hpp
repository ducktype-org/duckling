#pragma once

#include "../../rift_parser_state.hpp"

#include <diagnostic/source_position.hpp>

#include <token_parser_core/token_stream.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/automatic.hpp>

#include <base/unique_pointer.hpp>
#include <base/string_id.hpp>

#include <unicode/unistr.h>

#include "meta.hpp"
#include "not_statement.hpp"

namespace pst {
	class Attribute final: public Stmt {
		tpc::Identifier       name;
		ParserRef<AtrArgList> args = nullptr;

	public:
		STMT_CHILD_CONSTRUCTOR(Attribute);
		static ParserRef<Attribute> parse(RiftParserState& state);
		~Attribute() final = default;

		void dprint(std::ostream& out) const final;
		bool trailingSemicolon() override;

		[[nodiscard]]
		std::string elementType() const override {
			return "Attribute";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	/**
	 * @note: Import allows for two syntaxes right now:
	 * import A.B as D;
	 * import A.B.* as D;
	 *
	 * the optional "star" is ignored.
	 */
	class Import final: public Stmt {
		ParserRef<DottedName> names;
		tpc::Identifier       alias;

	public:
		STMT_CHILD_CONSTRUCTOR(Import);
		static ParserRef<Import> parse(RiftParserState& state);
		[[nodiscard]]
		const decltype(names)& getNames() const;

		[[nodiscard]]
		base::StrId getAlias() const {
			return alias.value;
		}

		/**
		 * @note In the future this functionality will be done by HELIOS.
		 * This functionality is needed to implement early import system for testing.
		 */
		[[nodiscard]]
		std::vector<base::StrId> getModulePath() const;

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
		ParserRef<DottedName> names;

	public:
		STMT_CHILD_CONSTRUCTOR(Using);
		static ParserRef<Using> parse(RiftParserState& state);

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
		ParserRef<Expr> expression;

	public:
		STMT_CHILD_CONSTRUCTOR(ExprStmt);
		static ParserRef<ExprStmt> parse(RiftParserState& state);

		~ExprStmt() final = default;
		void dprint(std::ostream& out) const final;

		void acceptVisitor(PstStmtVisitor& visitor) const override;

		[[nodiscard]]
		ParserCBorrowRef<Expr> getExpr() const {
			return expression.borrow();
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Expr Stmt";
		}
	};

	class Alias final: public Stmt {
		tpc::Identifier       name;
		ParserRef<DottedName> points_to;

	public:
		STMT_CHILD_CONSTRUCTOR(Alias);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		auto getPointed() const {
			return points_to->getNames();
		}

		static ParserRef<Alias> parse(RiftParserState& state);
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
		base::Optional<ParserRef<Expr>> expr;

	public:
		STMT_CHILD_CONSTRUCTOR(Action);
		static ParserRef<Action> parse(RiftParserState& state);
		~Action() override = default;

		[[nodiscard]]
		std::string elementType() const override {
			return "Action";
		}

		[[nodiscard]]
		base::Optional<ParserCBorrowRef<Expr>> getValue() const {
			return expr.map([](const auto& e) { return e.borrow(); });
		}
	};

	// TODO: Merge it with variable. Or perhaps make a new class DataStorage.
	class Const final: public Stmt {
		tpc::Identifier name;
		ParserRef<Expr> type;
		ParserRef<Expr> value;

	public:
		STMT_CHILD_CONSTRUCTOR(Const);
		static ParserRef<Const> parse(RiftParserState& state);

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		ParserCBorrowRef<Expr> getType() const {
			return type.borrow();
		}

		[[nodiscard]]
		ParserCBorrowRef<Expr> getValue() const {
			return value.borrow();
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
	class RiftTestingStmt: public Stmt {
	public:
		RiftTestingStmt(StmtKind kind, const dia::SourcePosition& position): Stmt(kind, position) {}
	};

#define RIFT_TEST_CHILD_CONSTRUCTOR(class_name)      \
	class_name(const dia::SourcePosition& position): \
		  RiftTestingStmt(StmtKind::class_name, position) {}

}
