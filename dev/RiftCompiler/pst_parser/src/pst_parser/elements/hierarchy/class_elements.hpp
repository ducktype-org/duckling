#pragma once

#include "not_statements.hpp"

namespace pst {
#define CLASS_STMT_CHILD_CONSTRUCTOR(class_name)                              \
	class_name(const dia::SourcePosition& position, const ClassContext& ctx): \
		  ClassStmt(StmtKind::class_name, position, ctx) {}

#define CLASS_STMT_PASS_CONSTRUCTOR(class_name)                                              \
	class_name(StmtKind kind, const dia::SourcePosition& position, const ClassContext& ctx): \
		  ClassStmt(kind, position, ctx) {}

#define CLASS_STMT_SPEC_CONSTRUCTOR(class_name)                               \
	class_name(const dia::SourcePosition& position, const ClassContext& ctx): \
		  ClassSpecial(StmtKind::class_name, position, ctx) {}

#define CLASS_STMT_PARSE(class_name) \
	static ParserRef<class_name> parse(RiftParserState& state, const ClassContext& ctx);

	/**
	 * @brief Access specifier block inside of a class.
	 *
	 * They are used to change the visibility of multiple definitions in a class
	 */
	class AccessBlock final: public ClassStmt {
		static inline const std::set<rift_def::Keyword> access_specifiers = {
			rift_def::Keyword::Public,
			rift_def::Keyword::Private,
			rift_def::Keyword::Protected,
		};

		rift_def::Keyword     specifier = rift_def::Keyword::NotAKeyword;
		ParserRef<ClassBlock> block;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(AccessBlock);
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
		ParserCBorrowRef<ClassBlock> getBlock() const {
			return block.borrow();
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return false;
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
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
		base::StrId getName() const {
			return kind.value;
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		[[nodiscard]]
		bool trailingSemicolon() override final {
			return false;
		}
	};

	class Constructor final: public ClassSpecial {
		ParserRef<ParamList> params = nullptr;
		ParserRef<InitList>  inits  = nullptr;
		ParserRef<CodeBlock> body   = nullptr;

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

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Destructor final: public ClassSpecial {
		ParserRef<CodeBlock> body = nullptr;

	public:
		CLASS_STMT_SPEC_CONSTRUCTOR(Destructor);
		CLASS_STMT_PARSE(Destructor);

		~Destructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Destructor";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Method final: public ClassStmt {
		tpc::Identifier      name;
		ParserRef<ParamList> params = nullptr;
		ParserRef<RetList>   rets   = nullptr;
		ParserRef<CodeBlock> body   = nullptr;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(Method);
		CLASS_STMT_PARSE(Method);

		~Method() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Method";
		}

		[[nodiscard]]
		base::StrId getName() const {
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

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Field final: public ClassStmt {
		bool                            is_mutable = true;
		tpc::Identifier                 name;
		ParserRef<Expr>                 type;
		base::Optional<ParserRef<Expr>> init;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(Field);
		CLASS_STMT_PARSE(Field);

		~Field() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Field";
		}

		[[nodiscard]]
		base::StrId getName() const {
			return name.value;
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return true;
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return true;
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};
}
