// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
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
		TESTER_ADD_TEST(staticInterfaceTest);
	}

private:
	void classInterfaceTest() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/tsh/class_definitions")));
		const compiler::helios::SymID my_class_symbol = getChain("MyClass", root_scope).back();

		const AbstractType my_class_type
			= query::entryPoint<compiler::tsh::QueryClassType>(my_class_symbol);


		withContextDo([&](query::Context& ctx) {
			CRef my_class_interface = my_class_type.getInterface(ctx);

			// note: If the type interface is modified,
			// these values might need to be updated.

			assertTrue(
				my_class_interface->getElements().size() == 8,
				"There should be exactly six declared members, plus the generated constructor and "
				"the generated parameterless constructor"
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
		const auto deep_copied_sym   = getChain("DeepCopied", root_scope).back();
		const auto non_def_const_class_sym
			= getChain("NonDefaultConstructibleClass", root_scope).back();
		const auto complex_complex_class_sym
			= getChain("MoreComplexComplexClass", root_scope).back();
		const auto complex_non_defaultable_sym
			= getChain("MoreComplexNonDefaultable", root_scope).back();

		withContextDo([&](query::Context& ctx) {
			auto get_type = [&](const compiler::helios::SymID sym_id) {
				return SymbolType<>::withDefaults(ctx.query<compiler::tsh::QueryClassType>(sym_id));
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

			// Declares a copy constructor itself, so copying it is not trivial.
			const auto deep_copied = get_type(deep_copied_sym);
			assert_flags(deep_copied, true, true, true, false, "DeepCopied");
			ASSERT_HAS_VALUE(deep_copied.getType().getInterface(ctx)->getSpecialElement(
				MemberSpecialKind::CopyConstructor
			));
			ASSERT_NO_VALUE(get_type(trivial_class_sym)
			                    .getType()
			                    .getInterface(ctx)
			                    ->getSpecialElement(MemberSpecialKind::CopyConstructor));

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

	void staticInterfaceTest() {
		auto [_, root_scope] = getModule(fs::File(path("test_modules/classes")));

		const auto members_sym = getChain("Base", root_scope).back();
		const auto derived_sym = getChain("Derived", root_scope).back();

		const AbstractType members_type = query::entryPoint<QueryClassType>(members_sym);
		const AbstractType derived_type = query::entryPoint<QueryClassType>(derived_sym);

		withContextDo([&](query::Context& ctx) {
			CRef interface = members_type.getInterface(ctx);

			auto single = [&](std::string_view element_name) -> const InterfaceElement& {
				const auto& with_name = interface->getElementsWithName(base::StrID(element_name));
				assertEqual(
					1u,
					with_name.size(),
					base::strConcat("There should be exactly one `", element_name, "` element.")
				);
				return with_name.front();
			};

			const auto& inst    = single("pub_field");
			const auto& counter = single("counter");
			const auto& get     = single("getPub");
			const auto& make    = single("make");

			ASSERT_TRUE(inst.isField());
			ASSERT_TRUE(not inst.isStaticField());
			ASSERT_TRUE(inst.isAnyField());
			ASSERT_TRUE(not inst.isAnyMethod());

			ASSERT_TRUE(not counter.isField());
			ASSERT_TRUE(counter.isStaticField());
			ASSERT_TRUE(counter.isAnyField());
			ASSERT_TRUE(not counter.isAnyMethod());

			ASSERT_TRUE(get.isMethod());
			ASSERT_TRUE(not get.isStaticMethod());
			ASSERT_TRUE(get.isAnyMethod());
			ASSERT_TRUE(not get.isAnyField());

			ASSERT_TRUE(not make.isMethod());
			ASSERT_TRUE(make.isStaticMethod());
			ASSERT_TRUE(make.isAnyMethod());
			ASSERT_TRUE(not make.isAnyField());

			// The declared interface holds only what the class itself declares, plus the
			// generated constructors and destructor.
			ASSERT_EQUAL(4, std::ranges::distance(interface->getFieldsView()));
			ASSERT_EQUAL(1, std::ranges::distance(interface->getStaticFieldsView()));
			ASSERT_EQUAL(5, std::ranges::distance(interface->getAnyFieldsView()));

			// `getPub`, `setPub`, `insidePrivate`, the user destructor and the generated one.
			ASSERT_EQUAL(5, std::ranges::distance(interface->getMethodsView()));
			// `make`, the copy constructor and the two generated constructors.
			ASSERT_EQUAL(4, std::ranges::distance(interface->getStaticMethodsView()));
			ASSERT_EQUAL(9, std::ranges::distance(interface->getAnyMethodsView()));

			ASSERT_HAS_VALUE(interface->getSpecialElement(MemberSpecialKind::CopyConstructor));
			ASSERT_HAS_VALUE(interface->getSpecialElement(MemberSpecialKind::UserDestructor));
			ASSERT_HAS_VALUE(interface->getSpecialElement(MemberSpecialKind::Constructor));
			ASSERT_HAS_VALUE(
				interface->getSpecialElement(MemberSpecialKind::ParameterlessConstructor)
			);
			ASSERT_NO_VALUE(interface->getSpecialElement(MemberSpecialKind::None));

			auto by_sym = interface->getElementBySym(counter.getSymbol());
			ASSERT_HAS_VALUE(by_sym);
			ASSERT_EQUAL(counter.getSymbol(), by_sym.value()->getSymbol());
			ASSERT_NO_VALUE(interface->getElementBySym(members_sym));

			// A class declares the visibility of every member, or of none of them.
			ASSERT_EQUAL(MemberVisibility::Public, inst.getVisibility());
			ASSERT_EQUAL(MemberVisibility::Private, single("priv_field").getVisibility());
			ASSERT_EQUAL(MemberVisibility::Protected, single("prot_field").getVisibility());
			ASSERT_EQUAL(MemberVisibility::Private, single("unspecified").getVisibility());
			ASSERT_EQUAL(members_type, inst.getSource());

			// A class is described by its own interface, which a class inheriting from it does
			// not merge into itself.
			CRef        derived_interface = derived_type.getInterface(ctx);
			const auto& derived_field
				= derived_interface->getElementsWithName(base::StrID("derived_field"));
			ASSERT_EQUAL(1u, derived_field.size());
			ASSERT_EQUAL(derived_type, derived_field.front().getSource());
			ASSERT_TRUE(derived_interface->getElementsWithName(base::StrID("pub_field")).empty());
			ASSERT_NO_VALUE(interface->getElementBySym(derived_field.front().getSymbol()));
		});
	}


public:
	~TypeSystemClassFieldsTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
