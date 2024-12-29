#pragma once

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

		lang_def::Keyword specifier = lang_def::Keyword::NotAKeyword;
		MBox<ClassBlock>  block;

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
		MCRef<ClassBlock> getBlock() const {
			return block.ref();
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
		tpc::Identifier kind;  ///< What is after the `.`

	public:
		CLASS_STMT_PASS_CONSTRUCTOR(ClassSpecial);
		CLASS_STMT_PARSE(ClassSpecial);

		[[nodiscard]]
		std::string elementType() const override {
			return "Class special method";
		}

		[[nodiscard]]
		base::StrID getName() const {
			return kind.value;
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

	class Constructor final: public ClassSpecial {
		MBox<ParamList> params = nullptr;
		MBox<InitList>  inits  = nullptr;
		MBox<CodeBlock> body   = nullptr;

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

	class Destructor final: public ClassSpecial {
		MBox<CodeBlock> body = nullptr;

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

	class Method final: public ClassStmt {
		tpc::Identifier                   name;
		MBox<ParamList>                   params = nullptr;
		base::Optional<MBox<ExprElement>> ret;
		MBox<CodeBlock>                   body = nullptr;

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

	class Field final: public ClassStmt {
		bool                              is_mutable = true;
		tpc::Identifier                   name;
		MBox<ExprElement>                 type;
		base::Optional<MBox<ExprElement>> init;

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
		MCRef<ExprElement> getType() const {
			return type.ref();
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
