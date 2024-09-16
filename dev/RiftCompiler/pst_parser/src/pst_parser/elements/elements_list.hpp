#pragma once

namespace pst {
	// Meta
	class Stmt;
	class NotStmt;
	class ClassStmt;
	// Lists
	class ParamList;
	class ImplementsList;
	class AtrArgList;
	class InitList;
	// Not Statements
	class FunParam;
	class DottedName;
	class Attribute;
	class ClassBlock;
	class ClassBlockOrStmt;
	class RoundGroupExpr;
	class Expr;
	// Statements
	class Import;
	class Using;
	class ExprStmt;
	class Alias;
	class Action;
	class Decl;
	class RiftTestingStmt;
	// Declarations
	class CodeDecl;
	class TopLevel;
	class Block;
	class Namespace;
	class Class;
	class Variable;
	class Const;
	class Fun;
	class If;
	class While;
	class For;
	// Actions
	class Return;
	class Continue;
	class Redo;
	class Restart;
	class Defer;
	class Throw;
	class Break;
	// Class Elements
	class AccessBlock;
	class ClassSpecial;
	class Constructor;
	class Destructor;
	class Method;
	class Field;

	// Expr Elements
	class ExprElement;
	class NewExprStmt;

	namespace expr {
		class PrefixOperator;
		class SuffixOperator;
		class BinaryOperator;
		class Value;
		class Literal;
		class IdentifierLiteral;
		class Access;
		class Call;
		class ChainExpr;
		class RoundExpr;
		class BlockExpr;
		class GeneralPrefix;
		class GeneralSuffix;
		class GeneralBinary;
		class ComparisonChain;
		class LogicNot;
		class LogicAnd;
		class LogicOr;
		class Ternary;
		class Comma;
		class Assignment;
	}
}
