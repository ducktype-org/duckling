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

#include <set>

namespace pst {
	class PstStmtVisitor;
	class Attribute;

	using StateCondition = bool(const RiftParserState&, i64);

	using GetName = std::string (*)();

	enum class StmtKind {
		Import,
		Using,
		Alias,
		Fun,
		Namespace,
		CodeDecl,
		Action,
		ExprStmt,
		Class,
		ClassStmt,
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

		std::vector<ParserRef<Attribute>> attributes;

		/**
		 * @brief Prepends attributes after parsing handling sub elements and position.
		 */
		void addAttributes(std::vector<ParserRef<Attribute>>&& additions);

		void dprintAttributes(std::ostream& out) const;

		void dprintPrefix(std::ostream& out) const override {
			dprintAttributes(out);
			RiftElement::dprintPrefix(out);
		}

	public:
		[[nodiscard]]
		StmtKind getKind() const {
			return kind;
		}

		static ParserRef<Stmt> parse(RiftParserState& state);
		bool                   trailingSemicolon() override;
		virtual void           acceptVisitor(PstStmtVisitor& visitor) const = 0;

		/** 
		 * @note This might need to return a vector of borrow pointers instead
		 */
		auto& getAttributes() const {
			return attributes;
		}

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

	class ClassStmt: public Stmt {
	protected:
		inline static const std::set<rift_def::Keyword> class_specs = {
			rift_def::Keyword::Public,
			rift_def::Keyword::Private,
			rift_def::Keyword::Static,
			rift_def::Keyword::Const,
		};
		std::vector<rift_def::Keyword> specifiers;
	
		void parseSpecifiers(RiftParserState& state);

		[[nodiscard]]
		i64 countSpecifiers(RiftParserState& state) const;

		void dprintPrefix(std::ostream& out) const override;
	public:
		STMT_CHILD_CONSTRUCTOR(ClassStmt);

		static ParserRef<ClassStmt> parse(RiftParserState& state);


		[[nodiscard]]
		std::string elementType() const override {
			return "Class Element";
		}
	};

}
