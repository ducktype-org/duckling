#pragma once

#include "elements.hpp"

#include <base/extend_cpp/visitor.hpp>

namespace compiler::helios::code {
	MAKE_VISITOR(HoutStmt,
		ReturnStmt,
		VoidReturnStmt,
		ExprStmt,
		IfStmt,
		WhileStmt,
		VariableStmt,
		AssignmentStmt
	);

	class HoutExprVisitor {
	public:
		virtual ~HoutExprVisitor()                                                      = default;
		virtual void visitLiteralUnitExpr(const LiteralUnitExpr&)                       = 0;
		virtual void visitLiteralIntExpr(const LiteralIntExpr&)                         = 0;
		virtual void visitLiteralBoolExpr(const LiteralBoolExpr&)                       = 0;
		virtual void visitLiteralStringExpr(const LiteralStringExpr&)                   = 0;
		virtual void visitLiteralTypeExpr(const LiteralTypeExpr&)                       = 0;
		virtual void visitIdentifierExpr(const IdentifierExpr&)                         = 0;
		virtual void visitBinaryOperatorExpr(const BinaryOperatorExpr&)                 = 0;
		virtual void visitUnaryOperatorExpr(const UnaryOperatorExpr&)                   = 0;
		virtual void visitTernaryOperatorExpr(const TernaryOperatorExpr&)               = 0;
		virtual void visitChainComparisonExpr(const ChainComparisonExpr&)               = 0;
		virtual void visitParenthesisExpr(const ParenthesisExpr&)                       = 0;
		virtual void visitTupleTypeConstructorExpr(const TupleTypeConstructorExpr&)     = 0;
		virtual void visitVariantTypeConstructorExpr(const VariantTypeConstructorExpr&) = 0;
		virtual void visitCallExpr(const CallExpr&)                                     = 0;
		virtual void visitAccessExpr(const AccessExpr&)                                 = 0;
		virtual void visitSequenceExpr(const SequenceExpr&)                             = 0;
	};

	class HoutExprVisitorEmpty: public HoutExprVisitor {
	public:
		~HoutExprVisitorEmpty() override = default;

		void visitLiteralUnitExpr(const LiteralUnitExpr&) override {}

		void visitLiteralIntExpr(const LiteralIntExpr&) override {}

		void visitLiteralBoolExpr(const LiteralBoolExpr&) override {}

		void visitLiteralStringExpr(const LiteralStringExpr&) override {}

		void visitLiteralTypeExpr(const LiteralTypeExpr&) override {}

		void visitIdentifierExpr(const IdentifierExpr&) override {}

		void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override {}

		void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override {}

		void visitTernaryOperatorExpr(const TernaryOperatorExpr&) override {}

		void visitChainComparisonExpr(const ChainComparisonExpr&) override {}

		void visitParenthesisExpr(const ParenthesisExpr&) override {}

		void visitTupleTypeConstructorExpr(const TupleTypeConstructorExpr&) override {}

		void visitVariantTypeConstructorExpr(const VariantTypeConstructorExpr&) override {}

		void visitCallExpr(const CallExpr&) override {}

		void visitAccessExpr(const AccessExpr&) override {}

		void visitSequenceExpr(const SequenceExpr&) override {}
	};

	class HoutExprVisitorPanicky: public HoutExprVisitor {
	public:
		~HoutExprVisitorPanicky() override = default;

		void visitLiteralUnitExpr(const LiteralUnitExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"LiteralUnitExpr"
				)
			);
		}

		void visitLiteralIntExpr(const LiteralIntExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"LiteralIntExpr"
				)
			);
		}

		void visitLiteralBoolExpr(const LiteralBoolExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"LiteralBoolExpr"
				)
			);
		}

		void visitLiteralStringExpr(const LiteralStringExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"LiteralStringExpr"
				)
			);
		}

		void visitLiteralTypeExpr(const LiteralTypeExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"LiteralTypeExpr"
				)
			);
		}

		void visitIdentifierExpr(const IdentifierExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"IdentifierExpr"
				)
			);
		}

		void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"BinaryOperatorExpr"
				)
			);
		}

		void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"UnaryOperatorExpr"
				)
			);
		}

		void visitTernaryOperatorExpr(const TernaryOperatorExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"TernaryOperatorExpr"
				)
			);
		}

		void visitChainComparisonExpr(const ChainComparisonExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"ChainComparisonExpr"
				)
			);
		}

		void visitParenthesisExpr(const ParenthesisExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"ParenthesisExpr"
				)
			);
		}

		void visitTupleTypeConstructorExpr(const TupleTypeConstructorExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"TupleTypeConstructorExpr"
				)
			);
		}

		void visitVariantTypeConstructorExpr(const VariantTypeConstructorExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"VariantTypeConstructorExpr"
				)
			);
		}

		void visitCallExpr(const CallExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"CallExpr"
				)
			);
		}

		void visitAccessExpr(const AccessExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"AccessExpr"
				)
			);
		}

		void visitSequenceExpr(const SequenceExpr&) override {
			throw base ::Panic(
				"    In "
				"/home/poleszc/ducktype/duckling/dev/src/compiler/core/helios/src/helios/hout/"
				"visitors.hpp"
				":"
				"34",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<HoutExprVisitorPanicky>(),
					" visitor has visited: "
					"SequenceExpr"
				)
			);
		}
	};
}
