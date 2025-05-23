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
	class ExprElement;
	// Statements
	class Import;
	class Using;
	class ExprStmt;
	class Alias;
	class Action;
	class Decl;
	class Expand;
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
	class CopyConstructor;
	class Destructor;
	class Method;
	class Field;

	namespace expr {
		class PrefixOperator;
		class SuffixOperator;
		class BinaryOperator;
		class ExprValue;
		class ExprStrValue;
		class ExprCharValue;
		class Literal;
		class TemplateSpecifier;
		class IdentifierLiteral;
		class KeywordLiteral;
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

	// Expr Holders

	class ExprHolder;
	class UniversalExprHolder;
	class UniversalExprHolderLowerLevel;
	class CommaExprHolder;
	class AssignmentExprHolder;
	class ForTypeExprHolder;
}
