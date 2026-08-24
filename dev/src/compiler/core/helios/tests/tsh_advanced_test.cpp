#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::tsh;
using namespace compiler::helios::test_utils;
using query::utils::withContextDo;

class TypeSystemClassFieldsTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TypeSystemClassFieldsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(classInterfaceTest);
		TESTER_ADD_TEST(classConstructabilityTest);
		TESTER_ADD_TEST(typeTemplateTest);
	}

private:
	void classInterfaceTest() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/tsh/class_definitions")));
		const compiler::helios::SymID my_class_symbol = getChain("MyClass", root_scope).back();

		const AbstractType my_class_type
			= query::entryPoint<compiler::helios::QueryTypeFromDefinition>(my_class_symbol)
		          ->valueOrPanicMsg("Not expecting an ERROR here...")
		          .getType();


		withContextDo([&](query::Context& ctx) {
			CRef my_class_interface = my_class_type.getInterface(ctx);

			// note: If the type interface is modified,
			// these values might need to be updated.

			assertTrue(
				my_class_interface->getElements().size() == 7,
				"There should be exactly six members, plus the generated destructor"
			);
			assertTrue(
				my_class_interface->getElementsByName().size() == 5,
				"There should be exactly four unique names, plus the generated destructor"
			);

			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("a")).size() == 1,
				"There should be exactly one 'a' member."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("b")).size() == 1,
				"There should be exactly one 'b' member."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("c")).empty(),
				"There should be exactly no 'c' members."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("getA")).size() == 1,
				"There should be exactly one 'getA' member."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("add")).size() == 3,
				"There should be exactly three 'add' members."
			);
			assertTrue(
				my_class_interface->getElementsWithName(base::StrID("nonExistent")).empty(),
				"There should be exactly no 'nonExistent' members."
			);
		});
	}

	void classConstructabilityTest() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/tsh/class_definitions")));

		const auto trivial_class_sym = getChain("TrivialClass", root_scope).back();
		const auto my_class_sym      = getChain("MyClass", root_scope).back();
		const auto complex_class_sym = getChain("ComplexClass", root_scope).back();
		const auto non_def_const_class_sym
			= getChain("NonDefaultConstructibleClass", root_scope).back();
		const auto complex_complex_class_sym
			= getChain("MoreComplexComplexClass", root_scope).back();
		const auto complex_non_defaultable_sym
			= getChain("MoreComplexNonDefaultable", root_scope).back();

		withContextDo([&](query::Context& ctx) {
			auto get_type = [&](const compiler::helios::SymID sym_id) {
				return ctx.query<compiler::helios::QueryTypeFromDefinition>(sym_id)->valueOrPanicMsg(
					"Not expecting an ERROR here..."
				);
			};

			auto assert_flags = [&](const SymbolType<>& type,
			                        bool                def,
			                        bool                zero,
			                        bool                copy,
			                        bool                triv,
			                        std::string_view    msg) {
				assertTrue(
					type.isDefaultConstructible(ctx) == def,
					base::strConcat(msg, " isDefaultConstructible check failed")
				);
				assertTrue(
					type.isTriviallyZeroInitializable(ctx) == zero,
					base::strConcat(msg, " isTriviallyZeroInitializable check failed")
				);
				assertTrue(
					type.isCopyable(ctx) == copy, base::strConcat(msg, " isCopyable check failed")
				);
				assertTrue(
					type.isTriviallyCopyable(ctx) == triv,
					base::strConcat(msg, " isTriviallyCopyable check failed")
				);
			};

			// Just a POD, should be as trivial as it can get.
			const auto trivial_class = get_type(trivial_class_sym);
			assert_flags(trivial_class, true, true, true, true, "TrivialClass");

			// POD but it's field has an initial value so its not trivially zero initializable.
			const auto my_class = get_type(my_class_sym);
			assert_flags(my_class, true, false, true, true, "MyClass");

			// Has a field with a user copy constructor, so it's not trivially copyable.
			const auto complex_class = get_type(complex_class_sym);
			assert_flags(complex_class, true, true, true, false, "ComplexClass");

			// Has a `ref i64` field, so it's not default constructible, but trivially copyable.
			const auto non_def_const_class = get_type(non_def_const_class_sym);
			assert_flags(non_def_const_class, false, false, true, true, "NonDefaultConstructible");

			// Has a field with a user copy constructor deeper in the hierarchy, so it's not
			// trivially copyable.
			const auto complex_complex_class = get_type(complex_complex_class_sym);
			assert_flags(complex_complex_class, true, true, true, false, "MoreComplexComplexClass");

			// Has `ref i64` field deeper in the hierarchy, so it's not default constructible, but
			// trivially copyable.
			const auto complex_non_defaultable = get_type(complex_non_defaultable_sym);
			assert_flags(
				complex_non_defaultable, false, false, true, true, "MoreComplexNonDefaultable"
			);
		});
	}

	void typeTemplateTest() {
		auto [_, root_scope]    = getModule(fs::File(path("test_modules/tsh/class_definitions")));
		const auto template_sym = getChain("TemplateClass", root_scope).back();

		withContextDo([&](query::Context& ctx) {
			const AbstractType type = ctx.query<compiler::helios::QueryTypeOfSymbol>(template_sym)
			                              ->valueOrPanicMsg("Not expecting an ERROR here...")
			                              .getType();

			assertTrue(
				type.getKind() == Kind::TypeTemplate,
				"A template class symbol should have a TypeTemplate type."
			);

			const TypeTemplateAbstractType template_type = type;
			assertTrue(
				template_type.getSource() == template_sym,
				"The source of the type template should be the template symbol."
			);
		});
	}


public:
	~TypeSystemClassFieldsTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
