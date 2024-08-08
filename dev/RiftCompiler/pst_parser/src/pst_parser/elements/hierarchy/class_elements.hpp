#pragma once

#include "not_statements.hpp"

namespace pst {
#define CLASS_STMT_CHILD_CONSTRUCTOR(class_name) \
	class_name(const dia::SourcePosition& position): ClassStmt(StmtKind::class_name, position) {}

#define CLASS_STMT_PASS_CONSTRUCTOR(class_name) \
	class_name(StmtKind kind, const dia::SourcePosition& position): ClassStmt(kind, position) {}

	class AccessBlock final: public ClassStmt {
		rift_def::Keyword specifier = rift_def::Keyword::NotAKeyword;
		ParserRef<ClassBlock> block;	

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(AccessBlock);
		static ParserRef<AccessBlock> parse(RiftParserState& state, base::StrId name);

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
		bool trailingSemicolon() override;

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	/**
	 * @brief Special class methods like constructors, and destructors using the
	 * `ClassName.type(...)` syntax
	 */
	class ClassSpecial: public ClassStmt {
		tpc::Identifier kind; ///< What is after the `.`

	public:
		CLASS_STMT_PASS_CONSTRUCTOR(ClassSpecial);
		static ParserRef<ClassSpecial> parse(RiftParserState& state);

		[[nodiscard]]
		std::string elementType() const override {
			return "Class special method";
		}

		[[nodiscard]]
		bool isDeclaration() const override {
			return false;
		}

		[[nodiscard]]
		bool trailingSemicolon() override final {
			return false;
		}
	};

	class Constructor final: public ClassSpecial {
		ParserRef<ParamList> params = nullptr;
		ParserRef<InitList> inits = nullptr;
		ParserRef<CodeBlock> body = nullptr;

	public:
		Constructor(dia::SourcePosition pos): ClassSpecial(StmtKind::Constructor, pos) {};
		static ParserRef<Constructor> parse(RiftParserState& state);

		~Constructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Constructor";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Destructor final: public ClassSpecial {
		ParserRef<CodeBlock> body = nullptr;

	public:
		Destructor(dia::SourcePosition pos): ClassSpecial(StmtKind::Destructor, pos) {};
		static ParserRef<Destructor> parse(RiftParserState& state);

		~Destructor() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Destructor";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Method final: public ClassStmt {
		tpc::Identifier            name;
		ParserRef<ParamList> params = nullptr;
		ParserRef<RetList> rets = nullptr;
		ParserRef<CodeBlock> body = nullptr;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(Method);
		static ParserRef<Method> parse(RiftParserState& state);

		~Method() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Method";
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};

	class Field final: public ClassStmt {
		tpc::Identifier            name;
		ParserRef<Expr>            type;
		base::Optional<ParserRef<Expr>> init;

	public:
		CLASS_STMT_CHILD_CONSTRUCTOR(Field);
		static ParserRef<Field> parse(RiftParserState& state);

		~Field() override = default;
		void dprint(std::ostream& out) const final;

		[[nodiscard]]
		std::string elementType() const override {
			return "Class Method";
		}

		[[nodiscard]]
		bool trailingSemicolon() override {
			return true;
		}

		void acceptVisitor(PstStmtVisitor& visitor) const override;
	};
}
