#pragma once

#include <base/variant.hpp>

#include "declarations.hpp"
#include "not_statements.hpp"

namespace pst {
#define CLASS_STMT_CHILD_CONSTRUCTOR(class_name, kind)                        \
	class_name(const dia::SourcePosition& position, const ClassContext& ctx): \
		  ClassStmt(StmtKind::class_name, position, ctx) {                    \
		this->element_kind = kind;                                            \
	}

#define CLASS_STMT_PASS_CONSTRUCTOR(class_name)                                              \
	class_name(StmtKind kind, const dia::SourcePosition& position, const ClassContext& ctx): \
		  ClassStmt(kind, position, ctx) {}

#define CLASS_STMT_SPEC_CONSTRUCTOR(class_name)                               \
	class_name(const dia::SourcePosition& position, const ClassContext& ctx): \
		  ClassSpecial(StmtKind::class_name, position, ctx) {}

#define CLASS_STMT_PARSE(class_name) \
	static MBox<class_name> parse(LangParserState& state, const ClassContext& ctx);

	/**
	 * @brief Access specifier block inside of a class.
	 *
	 * They are used to change the visibility of multiple definitions in a class
	 */
	class AccessBlock final: public ClassStmt {
		static inline const std::set<lang_def::Keyword> access_specifiers = {
			lang_def::Keyword::Public,
			lang_def::Keyword::Private,
			lang_def::Keyword::Protected,
		};

		lang_def::Keyword          specifier = lang_def::Keyword::NotAKeyword;
		AccessInternal<ClassBlock> block;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(AccessBlock, ElementKind::AccessBlock);
		CLASS_STMT_PARSE(AccessBlock);

		~AccessBlock() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Access specification block";
		}

		[[nodiscard]]
		bool isDeclaration() const final {
			return false;
		}

		[[nodiscard]]
		AccessLocked<ClassBlock> getBlock() const {
			return block.give();
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return false;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Special class methods like constructors, and destructors using the
	 * `ClassName.type(...)` syntax
	 */
	class ClassSpecial: public ClassStmt {
	protected:
		std::variant<tpc::Identifier, Keyword>
			kind;  ///< What is after the `.`, It may be a keyword in some cases(for now it's only
		           ///< the move constructor)

	public:
		CLASS_STMT_PASS_CONSTRUCTOR(ClassSpecial);
		CLASS_STMT_PARSE(ClassSpecial);

		[[nodiscard]]
		std::string elementType() const override {
			return "Class special method";
		}

		[[nodiscard]]
		base::StrID getName() const {
			using namespace tpc;
			base::StrID res;
			VARIANT_VISIT(
				kind,
				VISIT_CASE(Identifier, ident, res = base::StrID(ident))
					VISIT_CASE(Keyword, key, res = keywordToStr(key))
			);
			return res;
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		[[nodiscard]]
		bool trailingSemicolon() final {
			return false;
		}
	};

	/**
	 * @brief Class constructor element.
	 */
	class Constructor final: public ClassSpecial {
		AccessInternal<ParamList> params;
		AccessInternal<InitList>  inits;
		AccessInternal<CodeBlock> body;

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(Constructor);
		CLASS_STMT_PARSE(Constructor);

		~Constructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Constructor";
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Class constructor element.
	 */
	class CopyConstructor final: public ClassSpecial {
		AccessInternal<ParamList> params;
		AccessInternal<InitList>  inits;
		AccessInternal<CodeBlock> body;

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(CopyConstructor);
		CLASS_STMT_PARSE(CopyConstructor);

		~CopyConstructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Copy Constructor";
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Class destructor element.
	 */
	class Destructor final: public ClassSpecial {
		AccessInternal<CodeBlock> body;

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(Destructor);
		CLASS_STMT_PARSE(Destructor);

		~Destructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Destructor";
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Class method element.
	 */
	class Method final: public ClassStmt {
		tpc::Identifier                                 name;
		AccessInternal<ParamList>                       params;
		base::Optional<AccessInternal<CommaExprHolder>> ret;
		AccessInternal<CodeBlock>                       body;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(Method, ElementKind::ClassMethod);
		CLASS_STMT_PARSE(Method);

		~Method() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Method";
		}

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return false;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};

	/**
	 * @brief Class field element.
	 */
	class Field final: public ClassStmt {
		bool                                            is_mutable = true;
		tpc::Identifier                                 name;
		AccessInternal<CommaExprHolder>                 type;
		base::Optional<AccessInternal<CommaExprHolder>> init;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(Field, ElementKind::ClassField);
		CLASS_STMT_PARSE(Field);

		~Field() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Field";
		}

		[[nodiscard]]
		base::StrID getName() const {
			return name.value;
		}

		[[nodiscard]]
		AccessLocked<ExprHolder> getType() const {
			return type.give();
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return true;
		}

		void acceptVisitor(PstVisitor& visitor) const override;
	};
}
