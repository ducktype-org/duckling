// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#pragma once

#include "../../elements/elements_list.hpp"
#include "../../lang_parser_element.hpp"
#include "../../pst_state_forward.hpp"
#include "../elements_common.hpp"
#include "../lang_state_unmethods.hpp"
#include "stmt_kind_fd.hpp"

#include <diagnostic/source_position.hpp>
#include <string_id/string_id.hpp>
#include <token_parser_core/automatic.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>

namespace pst {

	using TokenStreamCondition = bool(const tpc::TokenStream&, i64);

	using GetName = std::string (*)();

	/**
	 * @brief A general element that is a common ancestor for elements that aren't statements
	 */
	class NotStmt: public LangElement {
		PARENT_CLASS(LangElement);

	protected:
		ELEMENT_CLONE_DECL(NotStmt);

	public:
		explicit NotStmt(LangElementConstructionArgument state): LangElement(state) {}

		[[nodiscard]]
		std::string elementType() const override {
			return "Not Statement";
		}

		bool trailingSemicolon() override;
	};

	/**
	 * @brief Three declaration options:
	 *
	 * * None - This statement doesn't introduce any symbols. For example an expression statement or
	 * a return statement.
	 * * Symbol - This statement introduces a symbol. For example a function
	 * declaration, variable declaration, or an import/using that binds exactly one name.
	 * * Transparent - This statement contains or
	 * links somewhere where there might be introduced. For example a macro expansion, specifier
	 * block, or an import/using with a star, a nested list or several selectors.
	 */
	enum class DeclKind {
		None,
		Symbol,
		Transparent,
	};

	enum class StmtKind : int {
		Import,
		Using,
		Fun,
		FunDecl,
		Pattern,
		Namespace,
		CodeDecl,
		SpecifierBlock,
		Action,
		ExprStmt,
		Class,
		TopLevel,
		Const,
		Variable,
		Expand,
		TemplateStmt,
		// Class Statements
		Method,
		Field,
		Constructor,
		CopyConstructor,
		MoveConstructor,
		Destructor,
		ClassSpecifierBlock,
	};

	namespace internal {
		void makeImplicitReturn(MRef<Stmt>);
	}

	/**
	 * @brief A general element that is a common ancestor of all statements.
	 */
	class Stmt: public LangElement {
		PARENT_CLASS(LangElement);
		THIS_CLASS(Stmt);
		CLONE_SUBELEMENTS();

	public:
		ELEMENT_CLONE_DECL(Stmt, kind, implicit_return);

	protected:
		StmtKind kind;

		friend void internal::makeImplicitReturn(MRef<Stmt>);

	protected:
		using AttrList    = std::vector<AccessInternalAnonymous<Attribute>>;
		using AttrBoxList = std::vector<Box<Attribute>>;

		using SpecList    = std::vector<AccessInternalAnonymous<StmtSpecifier>>;
		using SpecBoxList = std::vector<Box<StmtSpecifier>>;

		struct Prefixes {
			AttrList attributes;
			SpecList specifiers;
		};

		struct PrefixBoxes {
			AttrBoxList attributes;
			SpecBoxList specifiers;
		};

		Prefixes prefixes;
		bool     implicit_return{};

		Stmt(StmtKind kind, LangElementConstructionArgument state):
			  LangElement(state),
			  kind(kind) {}

		static PrefixBoxes collectPrefixes(LangParserState& state);

		/**
		 * @brief Prepends prefixes after parsing handling sub elements and position.
		 */
		void addPrefixes(LangParserState& state, PrefixBoxes&& additions);

		/**
		 * @brief Prints prefixes(attributes and specifiers)
		 */
		void dprintPrefixes(std::ostream& out) const;

		void dprintPrefix(std::ostream& out) const override;

		void calcElementPathHashRecursive() override;

		void makeImplicitReturn() { implicit_return = true; }

	public:
		[[nodiscard]]
		StmtKind getStmtKind() const {
			return kind;
		}

		static MBox<Stmt> parse(LangParserState& state);
		bool              trailingSemicolon() override;
		void              acceptVisitor(PstVisitor& visitor) const override = 0;

		HashAlg& addGenericDataToHash(HashAlg&) const override;

		[[nodiscard]]
		auto getAttributes() const {
			std::vector<AccessLocked<Attribute>> attributes;
			for (auto& attr: prefixes.attributes) attributes.push_back(attr.give());
			return attributes;
		}

		/**
		 * @brief Returns a list of specifiers from last to first.
		 */
		[[nodiscard]]
		auto getSpecifiers() const {
			std::vector<AccessLocked<StmtSpecifier>> specifiers;
			for (auto& spec: prefixes.specifiers) specifiers.push_back(spec.give());
			std::ranges::reverse(specifiers);
			return specifiers;
		}

		[[nodiscard]]
		bool isStatement() const final {
			return true;
		}

		[[nodiscard]]
		bool isImplicitReturn() const {
			return implicit_return;
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
		 * * usings and imports that bind exactly one name
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
		virtual DeclKind isDeclaration() const {
			return DeclKind::None;
		}

		/**
		 * @brief Gets the symbol name used internally in pst for the purposes of guessing what
		 * symbol is defined by a statement.
		 *
		 * Shouldn't be used outside of pst as it violates access.
		 *
		 * Uses getDeclSymbolIdentifier as the base implementation that can be overriden if for
		 * example a keyword is used instead.
		 */
		[[nodiscard]]
		virtual base::Optional<base::StrID> getInternalSymbolName() const;

		/**
		 * @brief Get the identifier declared by a given statement if it exists.
		 *
		 * @pre Only meaningful when `isDeclaration() == DeclKind::Symbol`. For `None` and
		 * `Transparent` statements (e.g. `using a.b.*;`) it returns an empty optional.
		 */
		[[nodiscard]]
		virtual base::Optional<AccessLocked<IdentifierWrapper>> getDeclSymbolIdentifier() const {
			return {};
		}
	};

#define STMT_CHILD_CONSTRUCTOR(class_name, element_kind_)                                  \
	class_name(LangElementConstructionArgument state): Stmt(StmtKind::class_name, state) { \
		this->element_kind = element_kind_;                                                \
	}
}

#define PARSE_DECL() static MBox<ThisClass> parse(LangParserState& state)
