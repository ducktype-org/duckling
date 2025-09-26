#pragma once

#include <base/box.hpp>

namespace pst {
	// Meta
	class Stmt;
	class NotStmt;
	class ClassStmt;
	// Lists
	class ParamList;
	class ImplementsList;
	class TemplateList;
	class AtrArgList;
	class InitList;
	class CallList;
	class FlowPatternList;
	// Not Statements
	class Param;
	class DottedName;
	class Attribute;
	class CodeBlock;
	class CodeBlockOrStmt;
	class ClassBlock;
	class ClassBlockOrStmt;
	class RoundGroupExpr;
	class ExprElement;
	class FlowPattern;
	class AnalysisPattern;
	class DeconstructorPattern;
	class TuplePattern;
	class WildcardPattern;
	class BindingPattern;
	class ValuePattern;
	// Statements
	class Import;
	class StmtSpecifier;
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
	class FunDecl;
	class Pattern;
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
		class Atom;
		class ChainExpr;
		class RoundExpr;
		class BlockExpr;
		class MatchExpr;
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

// find a better place for it?
DEFAULT_BOX_PTR_DELETER_DECLARATION(pst::ExprElement)
DEFAULT_BOX_PTR_DELETER_DECLARATION(pst::Attribute)
DEFAULT_BOX_PTR_DELETER_DECLARATION(pst::DottedName)
