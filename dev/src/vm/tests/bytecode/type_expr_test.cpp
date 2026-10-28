// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <tester/tester.hpp>

#include <vm/bytecode/type_expr.hpp>

#include <string>
#include <thread>
#include <vector>

/**
 * @brief What belongs here: interning and spelling of `vm::code::TypeExpr`. Equal expressions must
 * share one node, so that a pointer to a given pointee is one type however it was built.
 *
 * Resolving an expression to a `ValidTypeID` is tested with the type context, not here.
 */
class TypeExprTester: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TypeExprTester

	using TypeExpr = vm::code::TypeExpr;

	static std::string spell(const TypeExpr& expr) { return expr.canonicalName().str(); }

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(testDefaultIsInvalid);
		TESTER_ADD_TEST(testNamedInterning);
		TESTER_ADD_TEST(testDerivedInterning);
		TESTER_ADD_TEST(testCanonicalNames);
		TESTER_ADD_TEST(testKindAccessors);
		TESTER_ADD_TEST(testInvalidNamesPanic);
		TESTER_ADD_TEST(testConcurrentInterning);
	}

	void testDefaultIsInvalid() {
		const TypeExpr empty;
		ASSERT_TRUE(!empty.isValid());
		ASSERT_TRUE(TypeExpr::type(base::StrID("i64")).isValid());
		// Two expressions that denote no type are still equal to each other.
		ASSERT_TRUE(empty == TypeExpr());
		ASSERT_TRUE(!(empty == TypeExpr::type(base::StrID("i64"))));
	}

	void testNamedInterning() {
		ASSERT_TRUE(TypeExpr::type(base::StrID("i64")) == TypeExpr::type(base::StrID("i64")));
		ASSERT_TRUE(!(TypeExpr::type(base::StrID("i64")) == TypeExpr::type(base::StrID("i32"))));
		// The constructor and the named factory are the same thing.
		ASSERT_TRUE(TypeExpr(base::StrID("i64")) == TypeExpr::type(base::StrID("i64")));
	}

	/// The point of the whole exercise: one pointer type per pointee.
	void testDerivedInterning() {
		const auto dog = TypeExpr::type(base::StrID("Dog"));
		ASSERT_TRUE(TypeExpr::pointer(dog) == TypeExpr::pointer(dog));
		ASSERT_TRUE(
			TypeExpr::pointer(TypeExpr::type(base::StrID("Dog")))
			== TypeExpr::pointer(TypeExpr::type(base::StrID("Dog")))
		);

		// A pointer and a C pointer to the same pointee stay distinct.
		ASSERT_TRUE(!(TypeExpr::pointer(dog) == TypeExpr::cpointer(dog)));

		// Nesting depth is part of the identity.
		ASSERT_TRUE(!(TypeExpr::pointer(TypeExpr::pointer(dog)) == TypeExpr::pointer(dog)));

		ASSERT_TRUE(TypeExpr::cpointer() == TypeExpr::cpointer());
		ASSERT_TRUE(!(TypeExpr::cpointer() == TypeExpr::cpointer(dog)));

		ASSERT_TRUE(TypeExpr::dyntable(dog) == TypeExpr::dyntable(dog));
		ASSERT_TRUE(!(TypeExpr::dyntable(dog) == TypeExpr::pointer(dog)));
	}

	void testCanonicalNames() {
		const auto i64 = TypeExpr::type(base::StrID("i64"));
		ASSERT_EQUAL(std::string("i64"), spell(i64));
		ASSERT_EQUAL(std::string("ptr{i64}"), spell(TypeExpr::pointer(i64)));
		ASSERT_EQUAL(std::string("ptr{ptr{i64}}"), spell(TypeExpr::pointer(TypeExpr::pointer(i64))));
		ASSERT_EQUAL(std::string("cptr{i64}"), spell(TypeExpr::cpointer(i64)));
		ASSERT_EQUAL(std::string("cptr{}"), spell(TypeExpr::cpointer()));
		ASSERT_EQUAL(
			std::string("ptr{cptr{Dog}}"),
			spell(TypeExpr::pointer(TypeExpr::cpointer(TypeExpr::type(base::StrID("Dog")))))
		);
		ASSERT_EQUAL(std::string("dyntable{i64}"), spell(TypeExpr::dyntable(i64)));
		ASSERT_EQUAL(
			std::string("ptr{dyntable{ptr{i64}}}"),
			spell(TypeExpr::pointer(TypeExpr::dyntable(TypeExpr::pointer(i64))))
		);
	}

	void testKindAccessors() {
		using namespace vm::code::expr;

		const auto dog = TypeExpr::type(base::StrID("Dog"));
		ASSERT_TRUE(dog.isKind<Named>());
		ASSERT_TRUE(!dog.isKind<Pointer>());
		ASSERT_TRUE(dog.getKindAs<Named>()->name == base::StrID("Dog"));
		ASSERT_TRUE(dog.maybeGetKindAs<Pointer>().empty());

		const auto ptr_dog = TypeExpr::pointer(dog);
		ASSERT_TRUE(ptr_dog.isKind<Pointer>());
		ASSERT_TRUE(ptr_dog.getKindAs<Pointer>()->pointee == dog);

		const auto cptr_dog = TypeExpr::cpointer(dog);
		ASSERT_TRUE(cptr_dog.isKind<CPointer>());
		ASSERT_TRUE(cptr_dog.getKindAs<CPointer>()->pointee.has_value());
		ASSERT_TRUE(*cptr_dog.getKindAs<CPointer>()->pointee == dog);

		const auto void_ptr = TypeExpr::cpointer();
		ASSERT_TRUE(void_ptr.isKind<CPointer>());
		ASSERT_TRUE(void_ptr.getKindAs<CPointer>()->pointee.empty());

		const auto dogs = TypeExpr::dyntable(dog);
		ASSERT_TRUE(dogs.isKind<DynamicTable>());
		ASSERT_TRUE(dogs.getKindAs<DynamicTable>()->element == dog);
	}

	/// A name spelled like a derived expression, or an empty one inside `cptr{}`, would otherwise
	/// share that expression's node.
	void testInvalidNamesPanic() {
		for (const auto* name: { "", "ptr{Dog}", "cptr{}", "dyntable{i8}", "Dog}", "two words" }) {
			bool panicked = false;
			try {
				std::ignore = TypeExpr::type(base::StrID(name));
			} catch (base::Panic&) { panicked = true; }
			ASSERT_TRUE(panicked);
		}
	}

	/// The compiler lowers to bytecode on several query workers at once.
	void testConcurrentInterning() {
		constexpr usize          THREAD_COUNT = 8;
		constexpr usize          PER_THREAD   = 200;
		std::vector<std::thread> threads;
		std::vector<TypeExpr>    results(THREAD_COUNT, TypeExpr::type(base::StrID("placeholder")));

		threads.reserve(THREAD_COUNT);
		for (usize t = 0; t < THREAD_COUNT; ++t) {
			threads.emplace_back([&results, t]() {
				for (usize i = 0; i < PER_THREAD; ++i) {
					results.at(t
					) = TypeExpr::pointer(TypeExpr::pointer(TypeExpr::type(base::StrID("Shared"))));
				}
			});
		}
		for (auto& thread: threads) thread.join();

		// Every thread must have ended up with the one node for `ptr{ptr{Shared}}`.
		for (const auto& result: results) ASSERT_TRUE(result == results.front());
		ASSERT_EQUAL(std::string("ptr{ptr{Shared}}"), spell(results.front()));
	}
};

TESTER_COMMON_MAIN("/src/vm/tests/bytecode/");
