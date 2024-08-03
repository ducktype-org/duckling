#pragma once

#include "../../rift_parser_state.hpp"
#include "../elements_common.hpp"

#include <diagnostic/source_position.hpp>
#include <token_parser_core/token_stream.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/automatic.hpp>

#include <base/string_id.hpp>

#include <unicode/unistr.h>

namespace pst {
	class PstStmtVisitor;

	using StateCondition = bool(const RiftParserState&, i64);

	using GetName = std::string (*)();

	enum class StmtKind {
		Attribute,
		Import,
		Using,
		Alias,
		Fun,
		Namespace,
		CodeDecl,
		Action,
		ExprStmt,
		Struct,
		TopLevel,
		Const,
		Variable
	};

	class Stmt: public RiftElement {
		StmtKind kind;

	protected:
		Stmt(StmtKind kind, const dia::SourcePosition& position):
			  RiftElement(position),
			  kind(kind) {}

	public:
		[[nodiscard]]
		StmtKind getKind() const {
			return kind;
		}

		static ParserRef<Stmt> parse(RiftParserState& state);
		bool                   trailingSemicolon() override;
		virtual void           acceptVisitor(PstStmtVisitor& visitor) const = 0;

		[[nodiscard]]
		bool isStatement() const final {
			return true;
		}

		[[nodiscard]]
		std::string elementType() const override {
			return "Statement";
		}

		/**
		 * @brief Determines if given statement is a declaration.
		 * Declaration is everything that is considered a unique symbol in HELIOS.
		 * For example declarations are:
		 * * functions
		 * * classes
		 * * aliases and usings
		 * * ifs, whiles with a name
		 * * variable declaration
		 *
		 * For example declarations are not:
		 * * expressions
		 * * ifs, whiles without name
		 * * return, break
		 *
		 * @note: this definition of declaration might not
		 * always be equivalent to intuitive thinking about declarations.
		 */
		[[nodiscard]]
		virtual bool isDeclaration() const {
			return false;
		}
	};

#define STMT_CHILD_CONSTRUCTOR(class_name) \
	class_name(const dia::SourcePosition& position): Stmt(StmtKind::class_name, position) {}

	class NotStmt: public RiftElement {
	public:
		explicit NotStmt(const dia::SourcePosition& position): RiftElement(position) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Not Statement";
		}

		bool trailingSemicolon() override;
	};
}
