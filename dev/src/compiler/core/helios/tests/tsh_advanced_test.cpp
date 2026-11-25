// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <frontend/module_tree/module_tree_builder.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/tsh/types.hpp>

#include <base/str/str_utils.hpp>

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
		TESTER_ADD_TEST(sourceTypeNameTest);
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
				my_class_interface->getElements().size() == 14,
				"There should be exactly six declared members, plus the generated constructor, "
				"the generated parameterless constructor and the six comparison operators"
			);
			assertTrue(
				my_class_interface->getElementsByName().size() == 11,
				"There should be exactly four unique names, plus the generated destructor and "
				"the six comparison operators"
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
				return SymbolType<>::withDefaultsMut(ctx.query<compiler::tsh::QueryClassType>(sym_id
				));
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

			// `getPub`, `setPub`, `insidePrivate`, the user destructor, the generated one and the
			// six comparison operators.
			ASSERT_EQUAL(11, std::ranges::distance(interface->getMethodsView()));
			// `make`, the copy constructor and the two generated constructors.
			ASSERT_EQUAL(4, std::ranges::distance(interface->getStaticMethodsView()));
			ASSERT_EQUAL(15, std::ranges::distance(interface->getAnyMethodsView()));

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

	void sourceTypeNameTest() {
		constexpr std::string_view PERSON    = "class Person {}";
		constexpr std::string_view TYPES_NS  = R"(
			class Person {}
			namespace Types {
				class Inner {}
				template(T: type)
				class Boxed { value: T; }
			}
		)";
		constexpr std::string_view BOXED     = "template(T: type) class Boxed { value: T; }";
		constexpr std::string_view FLOAT_T   = R"(
			template(F: f64)
			class FloatT {}
			template(F: f32)
			class Float32T {}
		)";
		constexpr std::string_view OTHER_MOD = R"(
			class Person {}
			namespace NS { class Deep {} }
		)";

		checkSourceName("i64", "i64");
		checkSourceName("str", "str");
		checkSourceName("ptr Types.Inner", "ptr Types.Inner", TYPES_NS);
		checkSourceName("manyptr i64", "manyptr i64");
		checkSourceName("cptr i64", "cptr i64");
		checkSourceName("(ptr i64)[3]", "(ptr i64)[3]");
		checkSourceName("ptr (i64[3])", "ptr i64[3]");
		checkSourceName("i64[3][2]", "i64[3][2]");
		checkSourceName("(ptr i64)[3][2]", "(ptr i64)[3][2]");
		checkSourceName("(i64 | bool)[3]", "(bool | i64)[3]");
		checkSourceName("(i64, bool)", "(i64, bool)");
		checkSourceName("i64 | bool", "bool | i64");
		checkSourceName("(i64) -> bool", "(i64) -> bool");
		checkSourceName("funptr(i64, bool) -> ()", "funptr(i64, bool) -> ()");
		checkSourceName("funptr() -> i64", "funptr() -> i64");
		checkSourceName("(i64) -> bool | i64", "(i64) -> bool | i64");
		checkSourceName("(i64 | bool) -> i64", "(bool | i64) -> i64");
		checkSourceName("(ptr i64) -> ptr i64", "(ptr i64) -> ptr i64");
		checkSourceName("(i64) -> (i64) -> bool", "(i64) -> (i64) -> bool");
		checkSourceName("((i64) -> bool) -> i64", "((i64) -> bool) -> i64");
		checkSourceName("ptr ((i64) -> bool)", "ptr ((i64) -> bool)");
		checkSourceName("((i64) -> bool)[3]", "((i64) -> bool)[3]");
		checkSourceName("i64 | ((i64) -> bool)", "((i64) -> bool) | i64");
		checkSourceName("((i64) -> bool, i64)", "((i64) -> bool, i64)");
		checkSourceName("((i64) -> bool, i64) -> i64", "((i64) -> bool, i64) -> i64");
		checkSourceName("((i64) -> bool,)", "((i64) -> bool,)");
		checkSourceName("Person", "Person", PERSON);
		checkSourceName("Boxed[i64]", "Boxed[i64]", BOXED);
		checkSourceName("Types.Boxed[Person]", "Types.Boxed[Person]", TYPES_NS);
		checkSourceName(
			"Sized[3i64, bool]",
			"Sized[3, bool]",
			"template(N: i64, T: type) class Sized { value: T; }"
		);
		checkSourceName("FloatT[0.000000001]", "FloatT[1e-09f64]", FLOAT_T);
		checkSourceName("FloatT[0.000000002]", "FloatT[2e-09f64]", FLOAT_T);
		checkSourceName("FloatT[1.5]", "FloatT[1.5f64]", FLOAT_T);
		checkSourceName("Float32T[0.1f32]", "Float32T[0.1f32]", FLOAT_T);
		checkSourceName("FloatT[1e-09f64]", "FloatT[1e-09f64]", FLOAT_T);
		checkSourceName("om.Person", "Person", "import other_mod as om;", OTHER_MOD);
		checkSourceName("om.NS.Deep", "NS.Deep", "import other_mod as om;", OTHER_MOD);
		checkSourceName(
			"Boxed[om.Person]",
			"Boxed[Person]",
			base::strConcat("import other_mod as om;", BOXED),
			OTHER_MOD
		);

		const auto root_scope = buildSourceModule(
			R"(
				import other_mod as om;
				class Person {}
				namespace Types {
					template(T: type)
					class Boxed { value: T; }
				}
				template(T: type)
				class Boxed { value: T; }
				template(F: f64)
				class FloatT {}
				template(T: type)
				fun topFn(x: T) -> T = x;
				namespace FnNs {
					template(T: type)
					fun ident(x: T) -> T = x;
					template(N: i64)
					namespace Number { const value = N; }
				}
				var person: Person;
				var other_person: om.Person;
				var tiny_float: FloatT[0.000000001];
				var another_tiny_float: FloatT[0.000000002];
				var round_trip_tiny_float: FloatT[1e-09f64];
			)",
			OTHER_MOD
		);
		const auto person_type = getSymbolTypeOf("person", root_scope);
		ASSERT_TRUE(person_type != getSymbolTypeOf("other_person", root_scope));
		ASSERT_TRUE(
			getSymbolTypeOf("tiny_float", root_scope)
			!= getSymbolTypeOf("another_tiny_float", root_scope)
		);
		ASSERT_EQUAL(
			getSymbolTypeOf("tiny_float", root_scope),
			getSymbolTypeOf("round_trip_tiny_float", root_scope)
		);
		const std::vector<std::pair<std::string_view, std::string_view>> templates{
			{ "Boxed", "Type template: Boxed" },
			{ "Types.Boxed", "Type template: Types.Boxed" },
			{ "topFn", "Type template: topFn" },
			{ "FnNs.ident", "Type template: FnNs.ident" },
			{ "FnNs.Number", "Type template: FnNs.Number" },
		};
		std::vector<SymbolType<>> template_types;
		template_types.reserve(templates.size());
		for (const auto& [chain, _]: templates)
			template_types.push_back(getSymbolTypeOf(chain, root_scope));

		withContextDo([&](query::Context& ctx) {
			for (usize i = 0; i < templates.size(); ++i)
				assertEqual(
					std::string(templates[i].second),
					template_types[i].sourceTypeName(ctx),
					base::strConcat("Incorrect source type name for `", templates[i].first, "`")
				);
			const auto i64_type
				= getIntegralType(ctx, 64, IntegralAbstractType::Signedness::Signed);
			for (const auto bits: { 8u, 16u, 32u, 64u, 128u }) {
				for (const auto signedness: {
						 IntegralAbstractType::Signedness::Signed,
						 IntegralAbstractType::Signedness::Unsigned,
					 }) {
					const auto integral = getIntegralType(ctx, bits, signedness);
					const auto expected = base::strConcat(
						signedness == IntegralAbstractType::Signedness::Signed ? "i" : "u", bits
					);
					ASSERT_EQUAL(expected, integral.sourceTypeName(ctx));
					ASSERT_EQUAL(
						expected, SymbolType<>::withDefaultsMut(integral).sourceTypeName(ctx)
					);
				}
			}
			for (const auto bits: { 16u, 32u, 64u, 80u, 128u }) {
				const auto floating = getFloatType(ctx, bits);
				ASSERT_EQUAL(base::strConcat("f", bits), floating.sourceTypeName(ctx));
				ASSERT_EQUAL(
					base::strConcat("f", bits),
					SymbolType<>::withDefaultsMut(floating).sourceTypeName(ctx)
				);
			}
			ASSERT_EQUAL(std::string("()"), getUnitType().sourceTypeName(ctx));
			ASSERT_EQUAL(std::string("byte"), getByteType().sourceTypeName(ctx));
			ASSERT_EQUAL(std::string("bool"), getBoolType().sourceTypeName(ctx));
			ASSERT_EQUAL(std::string("char"), getCharType().sourceTypeName(ctx));
			const KeyFor_QueryTupleType single_tuple_key{
				.components = { SymbolType<>::withDefaultsMut(i64_type) },
			};
			const auto single_tuple = ctx.query<QueryTupleType>(single_tuple_key);
			assertEqual(
				std::string("(i64,)"),
				single_tuple.sourceTypeName(ctx),
				"Incorrect source type name for a single-element tuple"
			);

			assertEqual(
				std::string("i64"),
				i64_type.sourceTypeName(ctx),
				"AbstractType should expose the source-level type name too"
			);

			const SymbolType<> qualified_type(
				i64_type,
				ReferenceKind::Ref,
				Mutability::Immutable,
				Leakage::Leaking,
				Uniqueness::Unique
			);
			assertEqual(
				std::string("unique leaking imm ref i64"),
				qualified_type.sourceTypeName(ctx),
				"SymbolType qualifiers should use the same order as toString"
			);
			assertEqual(
				std::string("unique leaking ref i64"),
				qualified_type.withMutability(Mutability::Mutable).sourceTypeName(ctx),
				"Mutable symbol types should omit the imm qualifier"
			);

			assertEqual(
				std::string("void"),
				getVoidType().sourceTypeName(ctx),
				"Void should use its Duckling source spelling"
			);
			assertEqual(
				std::string("type"),
				getMetaType().sourceTypeName(ctx),
				"Meta should use its Duckling source spelling"
			);

			const auto function_type = ctx.query<QueryFunctionType>({
				.parameter_types = { SymbolType<>::withDefaultsMut(i64_type) },
				.result_type     = SymbolType<>::withDefaultsMut(getBoolType()),
			});
			assertEqual(
				std::string("funptr(i64) -> bool"),
				function_type.sourceTypeName(ctx),
				"Function types should use their Duckling source spelling"
			);
			const auto variant_type = ctx.query<QueryVariantType>({
				.underlying_types = {
					SymbolType<>::withDefaultsMut(i64_type),
					SymbolType<>::withDefaultsMut(getBoolType()),
				},
			});
			const auto composite_function_type = ctx.query<QueryFunctionType>({
				.parameter_types = { person_type },
				.result_type     = SymbolType<>::withDefaultsMut(variant_type),
			});
			assertEqual(
				std::string("funptr(Person) -> bool | i64"),
				composite_function_type.sourceTypeName(ctx),
				"Function components should use source-oriented names"
			);
		});
	}

	/**
	 * @brief Builds a module from @p source, with an optional `other_mod` submodule built from
	 * @p other_module, and returns its root scope.
	 */
	compiler::helios::ScopeID buildSourceModule(
		std::string_view source, std::string_view other_module
	) {
		const auto package_id  = base::StrID(base::generateRandomString(32));
		const auto make_module = [&](std::string_view name, std::string_view contents) {
			auto builder = compiler::frontend::ModuleTreeBuilder::create();
			builder->setPackageID(package_id);
			builder->setName(base::StrID(name));
			builder->setMainSourceFile(fs::FileManager::createRandomVirtualFile(contents, ".dk"));
			return builder;
		};
		auto module = make_module("source_type_names", source);
		if (!other_module.empty())
			module->addSubmodule(make_module("other_mod", other_module)->finalize());
		return getModuleScope(module->finalize()->getModuleID());
	}

	/**
	 * @brief Checks that a variable of type @p type_expression, declared after @p declarations,
	 * renders as @p expected.
	 */
	void checkSourceName(
		std::string_view type_expression,
		std::string_view expected,
		std::string_view declarations = "",
		std::string_view other_module = ""
	) {
		const auto type = getSymbolTypeOf(
			"checked",
			buildSourceModule(
				base::strConcat(declarations, "\nvar checked: ", type_expression, ";"), other_module
			)
		);
		withContextDo([&](query::Context& ctx) {
			assertEqual(
				std::string(expected),
				type.sourceTypeName(ctx),
				base::strConcat("Incorrect source type name for `", type_expression, "`")
			);
		});
	}

public:
	~TypeSystemClassFieldsTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
