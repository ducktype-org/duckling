#pragma once

#include "const_pool.hpp"

#include <base/extend_cpp/visitor.hpp>

namespace vm::code {

	class ConstVisitor {
	public:
		virtual ~ConstVisitor()                                                 = default;
		virtual void visitConstantU64(const ConstantU64&)                       = 0;
		virtual void visitConstantClass(const ConstantClass&)                   = 0;
		virtual void visitConstantFixedSizeTable(const ConstantFixedSizeTable&) = 0;
	};

	class ConstVisitorEmpty: public ConstVisitor {
	public:
		~ConstVisitorEmpty() override = default;

		void visitConstantU64(const ConstantU64&) override {}

		void visitConstantClass(const ConstantClass&) override {}

		void visitConstantFixedSizeTable(const ConstantFixedSizeTable&) override {}
	};

	class ConstVisitorPanicky: public ConstVisitor {
	public:
		~ConstVisitorPanicky() override = default;

		void visitConstantU64(const ConstantU64&) override {
			throw base ::Panic(
				"    In "
				"/home/wojtek/ducktype/duckling/dev/src/vm/src/vm/bytecode/const_pool_visitor.hpp"
				":"
				"13",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<ConstVisitorPanicky>(),
					" visitor has visited: "
					"ConstantU64"
				)
			);
		}

		void visitConstantClass(const ConstantClass&) override {
			throw base ::Panic(
				"    In "
				"/home/wojtek/ducktype/duckling/dev/src/vm/src/vm/bytecode/const_pool_visitor.hpp"
				":"
				"13",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<ConstVisitorPanicky>(),
					" visitor has visited: "
					"ConstantClass"
				)
			);
		}

		void visitConstantFixedSizeTable(const ConstantFixedSizeTable&) override {
			throw base ::Panic(
				"    In "
				"/home/wojtek/ducktype/duckling/dev/src/vm/src/vm/bytecode/const_pool_visitor.hpp"
				":"
				"13",
				base ::strConcat(
					"    Panic thrown:\n",
					"    ",
					base ::typeName<ConstVisitorPanicky>(),
					" visitor has visited: "
					"ConstantFixedSizeTable"
				)
			);
		}
	};
}
