// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <tester/tester.hpp>

#include <vm/bytecode/type_expr.hpp>
#include <vm/bytecode/type_of_data.hpp>
#include <vm/bytecode/validator/errors.hpp>
#include <vm/bytecode/validator/type_inserter.hpp>
#include <vm/bytecode/validator/type_resolver.hpp>
#include <vm/bytecode/validator/valid_type/type_context.hpp>

#include <tuple>
#include <vector>

/**
 * @brief What belongs here: the boundary between a type expression and the type it denotes.
 * `TypeContext` stores types, and only `TypeResolver` builds them and maps an expression onto one.
 *
 * The expression data model itself is tested in `type_expr_test.cpp`.
 */
class TypeResolverTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TypeResolverTester

	using TypeExpr = vm::code::TypeExpr;

	static vm::code::TypeOfData primitive(const char* name, usize size) {
		return vm::code::PrimitiveType(base::StrID(name), Bytes{ size });
	}

	/// A context holding `i64`, an `i64` pointer and a structure, with nothing else.
	static vm::code::TypeContext makeContext() {
		vm::code::TypeContext                   context;
		const std::vector<vm::code::TypeOfData> types{
			primitive("i64", 8),
			vm::code::PointerType(base::StrID("ptr_i64"), TypeExpr(base::StrID("i64"))),
			vm::code::DataType(
				base::StrID("Point"),
				{ vm::code::Field(base::StrID("x"), TypeExpr(base::StrID("i64"))) }
			),
		};
		vm::code::detail::insertTypes(context, types, {});
		return context;
	}

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testResolvesDeclaredName);
		TESTER_ADD_TEST(testResolveRejectsUnknownName);
		TESTER_ADD_TEST(testFindResolvedIsEmptyForUnknownName);
		TESTER_ADD_TEST(testInnerReferencesResolve);
		TESTER_ADD_TEST(testRedeclaringIsAccepted);
		TESTER_ADD_TEST(testTypesReferToLaterAndThemselves);
	}

	void testResolvesDeclaredName() {
		auto                   context = makeContext();
		vm::code::TypeResolver resolver(context);

		const auto i64 = resolver.resolve(TypeExpr(base::StrID("i64")));
		ASSERT_TRUE(context.getCurrentTypes().at(i64)->getName() == base::StrID("i64"));

		// Resolving the same expression twice gives the same type.
		ASSERT_TRUE(i64 == resolver.resolve(TypeExpr(base::StrID("i64"))));
	}

	void testResolveRejectsUnknownName() {
		auto                   context = makeContext();
		vm::code::TypeResolver resolver(context);

		assertThrows<vm::code::UnknownTypeError>(
			[&] { std::ignore = resolver.resolve(TypeExpr(base::StrID("NoSuchType"))); },
			"Resolving an undeclared name should report an unknown type"
		);
	}

	void testFindResolvedIsEmptyForUnknownName() {
		const auto  context = makeContext();
		const auto& types   = context.getCurrentTypes();

		ASSERT_TRUE(findResolved(types, TypeExpr(base::StrID("i64"))).has_value());
		ASSERT_TRUE(!findResolved(types, TypeExpr(base::StrID("NoSuchType"))).has_value());
	}

	/// A declaration's inner references go through the resolver too, so a pointee and a field
	/// type must come out as the very type that was declared under that name.
	void testInnerReferencesResolve() {
		const auto  context = makeContext();
		const auto& types   = context.getCurrentTypes();

		const auto i64
			= findResolved(types, TypeExpr(base::StrID("i64"))).expect("i64 was declared");
		const auto pointer
			= findResolved(types, TypeExpr(base::StrID("ptr_i64"))).expect("ptr_i64 was declared");

		const auto pointee = pointer->maybeGetKindAs<vm::code::valid_type::finalized::Pointer>();
		ASSERT_TRUE(pointee.has_value());
		ASSERT_TRUE(pointee.value()->inner == i64->getID());
	}

	void testRedeclaringIsAccepted() {
		vm::code::TypeContext                   context;
		const std::vector<vm::code::TypeOfData> types{ primitive("i64", 8) };
		vm::code::detail::insertTypes(context, types, {});

		// The same declaration arriving again is not an error; a program may be loaded in pieces.
		vm::code::detail::insertTypes(context, types, {});
		ASSERT_EQUAL(usize(1), context.getCurrentTypes().size());
	}

	/// A type is built the first time anything refers to it, so declaration order does not matter
	/// and a type may reach itself through a pointer.
	void testTypesReferToLaterAndThemselves() {
		vm::code::TypeContext                   context;
		const std::vector<vm::code::TypeOfData> types{
			vm::code::DataType(
				base::StrID("Node"),
				{ vm::code::Field(base::StrID("next"), TypeExpr(base::StrID("ptr_Node"))),
			      vm::code::Field(base::StrID("value"), TypeExpr(base::StrID("i64"))) }
			),
			vm::code::PointerType(base::StrID("ptr_Node"), TypeExpr(base::StrID("Node"))),
			primitive("i64", 8),
		};
		vm::code::detail::insertTypes(context, types, {});

		const auto& stored = context.getCurrentTypes();
		ASSERT_EQUAL(usize(3), stored.size());
		const auto node
			= findResolved(stored, TypeExpr(base::StrID("Node"))).expect("Node is built");
		const auto i64 = findResolved(stored, TypeExpr(base::StrID("i64"))).expect("i64 is built");
		ASSERT_TRUE(node->getSize().assumePointerSize(Bytes(8)) == Bytes(16));

		const auto pointer = findResolved(stored, TypeExpr(base::StrID("ptr_Node")))
		                         .expect("ptr_Node is built")
		                         ->maybeGetKindAs<vm::code::valid_type::finalized::Pointer>();
		ASSERT_TRUE(pointer.has_value());
		ASSERT_TRUE(pointer.value()->inner == node->getID());
		ASSERT_TRUE(i64->isInstantiable());
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/bytecode/");
