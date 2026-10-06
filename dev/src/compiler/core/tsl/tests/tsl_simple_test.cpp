// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <helios/test_utils/helios_test_utils.hpp>
#include <helios/tsh/queries.hpp>
#include <helios/tsh/type_interface.hpp>
#include <tsl/all.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace compiler::tsl;
using namespace compiler::tsh;
using query::utils::withContextDo;

namespace {
	CRef<TypeLayout> queryLayout(query::Context& ctx, AbstractType type) {
		return { &ctx.query<QueryAbstractTypeLayout>(type)->valueOrThrow() };
	}

	CRef<TypeLayout> querySymbolLayout(query::Context& ctx, SymbolType<> type) {
		return { &ctx.query<QuerySymbolTypeLayout>(type)->valueOrThrow() };
	}
}

class LowerTypeSystemSimpleTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LowerTypeSystemSimpleTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicTypesTest);
		TESTER_ADD_TEST(variantTest);
		TESTER_ADD_TEST(tupleTest);
		TESTER_ADD_TEST(staticArrayTest);
		TESTER_ADD_TEST(classTest);
		TESTER_ADD_TEST(classWithArrayFieldTest);
		TESTER_ADD_TEST(mutabilityTest);
		TESTER_ADD_TEST(pointerLayoutManglingTest);
	}

private:
	static void testPrinting(
		CRef<TypeLayout> layout, query::Context& ctx, const bool do_non_recursive = false
	) {
		std::cout << layout->toStringIdentification() << "\n";
		std::cout << layout->toStringDefinition(ctx) << "\n";
		if (do_non_recursive) {
			std::cout << "non recursive:\n";
			std::cout << layout->toStringDefinition(ctx, false) << "\n";
		}
	}

	using enum Mutability;

	static SymbolType<> st(const AbstractType abstract_type, const bool is_mutable = false) {
		return SymbolType{ abstract_type, ReferenceKind::Direct, is_mutable ? Mutable : Immutable };
	}

	void basicTypesTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const UnitAbstractType unit_type   = getUnitType();
			auto                   unit_layout = queryLayout(ctx, unit_type);

			assertTrue(unit_layout->getSize() == Bits(0), "Empty layout should have size zero.");
			assertTrue(
				unit_layout->getSourceType().getType() == getUnitType(),
				"Layout should have source type as constructed."
			);
			variant_match(unit_layout->getVariant()) {
				variant_case(EmptyTypeLayout, l) { /* good */ }
				variant_default { fail("Layout of unit type should be empty."); }
			}
			testPrinting(unit_layout, ctx);

			const std::array<std::pair<AbstractType, Bits>, 3> small_types{ {
				{ getBoolType(), BOOL_SIZE },
				{ getByteType(), BYTE_SIZE },
				{ getCharType(), CHAR_SIZE },
			} };
			for (auto [small_type, expected_small_size]: small_types) {
				auto small_layout = queryLayout(ctx, small_type);
				assertTrue(
					small_layout->getSize() == expected_small_size,
					"Integral layout should have size equal to that of the source type."
				);
				assertTrue(
					small_layout->getSourceType().getType() == small_type,
					"Layout should have source type as constructed."
				);
				variant_match(small_layout->getVariant()) {
					variant_case(IntegralTypeLayout, l) { /* good */ }
					variant_default { fail("Layout of byte sized type should be integral."); }
				}
				testPrinting(small_layout, ctx);
			}

			for (constexpr std::array<usize, 5> int_sizes{ 8, 16, 32, 64, 128 };
			     usize                          size: int_sizes) {
				IntegralAbstractType int_type = getIntegralType(
					ctx, size, compiler::tsh::IntegralAbstractType::Signedness::Signed
				);
				auto int_layout = queryLayout(ctx, int_type);

				assertTrue(
					int_layout->getSize() == Bits(size),
					"Integral layout should have size equal to that of the source type."
				);
				assertTrue(
					int_layout->getSourceType().getType() == int_type,
					"Layout should have source type as constructed."
				);
				variant_match(int_layout->getVariant()) {
					variant_case(IntegralTypeLayout, l) { /* good */ }
					variant_default { fail("Layout of integral type should be integral."); }
				}
				testPrinting(int_layout, ctx);
			}

			for (constexpr std::array<usize, 5> float_sizes{ 16, 32, 64, 80, 128 };
			     usize                          size: float_sizes) {
				FloatAbstractType float_type = getFloatType(ctx, size);

				auto float_layout = queryLayout(ctx, float_type);
				assertTrue(
					float_layout->getSize() == Bits(size),
					"Float layout should have size equal to that of the source type."
				);
				assertTrue(
					float_layout->getSourceType().getType() == float_type,
					"Layout should have source type as constructed."
				);
				variant_match(float_layout->getVariant()) {
					variant_case(FloatTypeLayout, l) { /* good */ }
					variant_default { fail("Layout of float type should be float."); }
				}
				testPrinting(float_layout, ctx);
			}

			const FunctionAbstractType function_type
				= ctx.query<QueryFunctionType>({ {}, st(unit_type) });
			auto functional_layout = queryLayout(ctx, function_type);
			assertTrue(
				functional_layout->getSize() == POINTER_SIZE,
				"Functional layout should have size equal to the size of a pointer."
			);
			assertTrue(
				functional_layout->getSourceType().getType() == function_type,
				"Layout should have source type as constructed."
			);
			variant_match(functional_layout->getVariant()) {
				variant_case(FunctionalTypeLayout, l) { /* good */ }
				variant_default { fail("Layout of function type should be functional."); }
			}
			testPrinting(functional_layout, ctx);

			const RawPointerAbstractType raw_pointer_type = getRawPointerType(false);

			auto raw_pointer_layout = queryLayout(ctx, raw_pointer_type);
			assertTrue(
				raw_pointer_layout->getSize() == POINTER_SIZE,
				"Raw pointer layout should have size equal to the size of a pointer."
			);
			assertTrue(
				raw_pointer_layout->getSourceType().getType() == raw_pointer_type,
				"Layout should have source type as constructed."
			);
			variant_match(raw_pointer_layout->getVariant()) {
				variant_case(PointerTypeLayout, l) {
					assertFalse(l.hasPointee(), "Raw pointer layout should not have pointee.");
				}
				variant_default { fail("Layout of raw pointer type should be pointer-like."); }
			}
			testPrinting(raw_pointer_layout, ctx);

			const PointerAbstractType unit_pointer_type
				= ctx.query<QueryPointerType>({ st(unit_type) });
			auto unit_pointer_layout = queryLayout(ctx, unit_pointer_type);
			assertTrue(
				unit_pointer_layout->getSize() == POINTER_SIZE,
				"Typed pointer layout should have size equal to the size of a pointer."
			);
			assertTrue(
				unit_pointer_layout->getSourceType().getType() == unit_pointer_type,
				"Layout should have source type as constructed."
			);
			variant_match(unit_pointer_layout->getVariant()) {
				variant_case(PointerTypeLayout, l) {
					assertTrue(l.hasPointee(), "Typed pointer layout should have pointee.");
					assertEqual(
						*l.getPointee(ctx),
						*unit_layout,
						"Pointee should be a layout of the pointed-to type."
					);
				}
				variant_default { fail("Layout of pointer type should be pointer-like."); }
			}
			testPrinting(unit_pointer_layout, ctx);
		});
	}

	void staticArrayTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const IntegralAbstractType i32_type
				= getIntegralType(ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed);
			const auto  i32_layout = queryLayout(ctx, i32_type);
			const usize count      = 10;

			const StaticArrayAbstractType static_array_type
				= ctx.query<QueryStaticArrayType>({ st(i32_type), count });

			const auto static_array_layout = queryLayout(ctx, static_array_type);

			assertEqual(
				static_array_layout->getSize(),
				Bits(32 * count),
				"Static array size should be exactly (element_size * count)."
			);

			assertEqual(
				static_array_layout->getSourceType().getType(),
				static_array_type,
				"Layout source type mismatch."
			);

			// The alignment of a static array must equal the element alignment,
			// not be derived from the total array size.
			assertEqual(
				static_array_layout->getAlignment(),
				i32_layout->getAlignment(),
				"Static array alignment should equal the element alignment."
			);

			variant_match(static_array_layout->getVariant()) {
				variant_case(StaticArrayTypeLayout, l) {
					assertEqual(l.getElementCount(), count, "Stored element count mismatch.");

					assertEqual(
						*l.getElementLayout(),
						*i32_layout,
						"Array element layout should match the base type layout."
					);
				}
				variant_default {
					fail("Layout of static array type should be StaticArrayTypeLayout variant.");
				}
			}

			testPrinting(static_array_layout, ctx, true);
		});
	}

	void variantTest() {
		withContextDo([&](query::Context& ctx) -> void {
			SymbolType<> i8_type = st(
				getIntegralType(ctx, 8, compiler::tsh::IntegralAbstractType::Signedness::Signed)
			);
			SymbolType<> f16_type = st(getFloatType(ctx, 16));

			const VariantAbstractType variant_type
				= ctx.query<QueryVariantType>({ { i8_type, f16_type } });
			auto variant_layout = queryLayout(ctx, variant_type);

			assertTrue(
				variant_layout->getSize() == BYTE_SIZE * 2 + Bits(16),
				"Variant layout size should account for data alignment."
			);
			assertTrue(
				variant_layout->getSourceType().getType() == variant_type,
				"Layout should have source type as constructed."
			);
			variant_match(variant_layout->getVariant()) {
				variant_case(VariantTypeLayout, l) {
					assertTrue(
						l.getTagOffset() == Bytes(0), "Variant tag should be at the beginning."
					);
					assertTrue(l.getTagSize() == BYTE_SIZE, "Variant tag should not be too big.");
					assertTrue(
						l.getDataOffset() == Bytes(2),
						"Data offset takes should take alignment into account."
					);
					assertTrue(
						l.getIndexOfType(i8_type) != l.getIndexOfType(f16_type),
						"Different variant options should have different indices."
					);
					assertTrue(
						l.getLayoutOfIndex(0)->getSourceType().getType()
							!= l.getLayoutOfIndex(1)->getSourceType().getType(),
						"Different indices should correspond to different types."
					);
				}
				variant_default { fail("Layout of variant type should be variant-like."); }
			}
			testPrinting(variant_layout, ctx, true);
		});
	}

	void tupleTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const SymbolType<> i8_type = st(
				getIntegralType(ctx, 8, compiler::tsh::IntegralAbstractType::Signedness::Signed)
			);
			const SymbolType<> f16_type = st(getFloatType(ctx, 16));
			const SymbolType<> f64_type = st(getFloatType(ctx, 64));
			const SymbolType<> f16_ref
				= SymbolType<>{ getFloatType(ctx, 16), ReferenceKind::Ref, Immutable };
			const SymbolType<> f16_box
				= SymbolType<>{ getFloatType(ctx, 16), ReferenceKind::Box, Immutable };
			const TupleAbstractType tuple_type   = ctx.query<QueryTupleType>({
                { i8_type, f16_type, f64_type, f16_ref, f16_box },
            });
			auto                    tuple_layout = queryLayout(ctx, tuple_type);

			assertTrue(
				tuple_layout->getSize() == BYTE_SIZE * 16 + POINTER_SIZE * 2,
				"Tuple layout size should account for data alignment and references."
			);
			assertTrue(
				tuple_layout->getSourceType().getType() == tuple_type,
				"Layout should have source type as constructed."
			);

			CRef<TypeInterface> tuple_interface = tuple_type.getInterface(ctx);

			const compiler::helios::SymID first = [&] {
				const auto& matching = tuple_interface->getElementsWithName(base::StrID("_1"));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			}();

			const compiler::helios::SymID second = [&] {
				const auto& matching = tuple_interface->getElementsWithName(base::StrID("_2"));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			}();

			const compiler::helios::SymID third = [&] {
				const auto& matching = tuple_interface->getElementsWithName(base::StrID("_3"));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			}();

			variant_match(tuple_layout->getVariant()) {
				variant_case(ClassTypeLayout, l) {
					assertTrue(
						l.getOffsetOfFieldSymbol(first) == Bytes(0)
							&& l.getOffsetOfFieldSymbol(second) == Bytes(2)
							&& l.getOffsetOfFieldSymbol(third) == Bytes(8),
						"Tuple layout should align its component layouts."
					);

					for (const auto& field: { first, second, third }) {
						assertTrue(
							l.getFieldSymbolOfLayoutIndex(
								l.getLayoutIndexOfFieldSymbol(field).value()
							) == field,
							"Layout field symbol to layout index mapping should be reversible."
						);
					}
				}
				variant_default { fail("Layout of tuple type should be class-like."); }
			}
			testPrinting(tuple_layout, ctx, true);
		});
	}

	void classTest() {
		using namespace compiler::helios;
		using namespace test_utils;

		auto [_, root_scope]        = getModule(fs::File(path("class_layout")));
		const SymID my_class_symbol = getChain("MyClass", root_scope).back();

		withContextDo([&](query::Context& ctx) -> void {
			const ClassAbstractType my_class_type      = ctx.query<QueryClassType>(my_class_symbol);
			CRef<TypeInterface>     my_class_interface = my_class_type.getInterface(ctx);

			const SymID a_field_symbol = [&] {
				const auto& matching = my_class_interface->getElementsWithName(base::StrID("a"));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			}();
			const SymID b_field_symbol = [&] {
				const auto& matching = my_class_interface->getElementsWithName(base::StrID("b"));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			}();
			const SymID c_field_symbol = [&] {
				const auto& matching = my_class_interface->getElementsWithName(base::StrID("c"));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			}();
			const SymID d_field_symbol = [&] {
				const auto& matching = my_class_interface->getElementsWithName(base::StrID("d"));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			}();
			const SymID e_field_symbol = [&] {
				const auto& matching = my_class_interface->getElementsWithName(base::StrID("e"));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			}();

			auto my_class_layout = queryLayout(ctx, my_class_type);
			assertTrue(
				// (1 + padding 1) + (2 + padding 2) + 8 + 8 + 8
				my_class_layout->getSize() == BYTE_SIZE * 32,
				"Class layout size should account for data alignment."
			);
			// The class alignment equals the max alignment of its members.
			// f64, ref i32, box f16 are all 8-byte aligned, so class alignment = 8.
			assertEqual(
				my_class_layout->getAlignment(),
				Bytes(8),
				"Class alignment should equal the maximum alignment of its members."
			);
			assertTrue(
				my_class_layout->getSourceType().getType() == my_class_type,
				"Layout should have source type as constructed."
			);

			variant_match(my_class_layout->getVariant()) {
				variant_case(ClassTypeLayout, l) {
					assertTrue(
						l.getOffsetOfFieldSymbol(a_field_symbol) == Bytes(0)
							&& l.getOffsetOfFieldSymbol(b_field_symbol) == Bytes(2)
							&& l.getOffsetOfFieldSymbol(c_field_symbol) == Bytes(8)
							&& l.getOffsetOfFieldSymbol(d_field_symbol) == Bytes(16)
							&& l.getOffsetOfFieldSymbol(e_field_symbol) == Bytes(24),
						"Class layout should align its component layouts."
					);
				}
				variant_default { fail("Layout of class type should be class-like."); }
			}
			testPrinting(my_class_layout, ctx, true);
		});
	}

	void classWithArrayFieldTest() {
		using namespace compiler::helios;
		using namespace test_utils;

		// This test considers a class containing a static array to check if the element
		// alignment is computed correctly.
		auto [_, root_scope]     = getModule(fs::File(path("class_layout_with_array")));
		const SymID class_symbol = getChain("ClassWithArray", root_scope).back();

		withContextDo([&](query::Context& ctx) -> void {
			const ClassAbstractType class_type      = ctx.query<QueryClassType>(class_symbol);
			CRef<TypeInterface>     class_interface = class_type.getInterface(ctx);

			auto get_element_symbol = [&](const std::string& name) {
				const auto& matching = class_interface->getElementsWithName(base::StrID(name));
				ASSERT_TRUE(matching.size() == 1);
				return matching.at(0).getSymbol();
			};

			const SymID element1_symbol = get_element_symbol("element1");
			const SymID element2_symbol = get_element_symbol("element2");
			const SymID element3_symbol = get_element_symbol("element3");
			const SymID elements_symbol = get_element_symbol("elements");

			const auto class_layout = queryLayout(ctx, class_type);

			// element1(4) + element2(4) + element3(4) + elements(8) = 20 bytes.
			// No padding needed because all fields are 4-byte aligned.
			assertEqual(
				class_layout->getSize(),
				BYTE_SIZE * 20,
				"Class layout size should account for array element alignment."
			);

			variant_match(class_layout->getVariant()) {
				variant_case(ClassTypeLayout, l) {
					// elements: i32[2] must be placed at offset 12 (4-byte aligned),
					// not offset 16 (8-byte aligned), because the array's alignment is
					// determined by its element type (i32 = 4 bytes), not by its total
					// size (8 bytes).
					assertTrue(
						l.getOffsetOfFieldSymbol(element1_symbol) == Bytes(0)
							&& l.getOffsetOfFieldSymbol(element2_symbol) == Bytes(4)
							&& l.getOffsetOfFieldSymbol(element3_symbol) == Bytes(8)
							&& l.getOffsetOfFieldSymbol(elements_symbol) == Bytes(12),
						"Array field offset must use element alignment, not total array size."
					);
				}
				variant_default { fail("Layout of Database class should be class-like."); }
			}
			testPrinting(class_layout, ctx, true);
		});
	}

	void mutabilityTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto int_symbol_type = st(
				getIntegralType(ctx, 64, compiler::tsh::IntegralAbstractType::Signedness::Signed),
				true
			);
			auto const_int_symbol_type = int_symbol_type.withMutability(Immutable);
			auto int_layout            = querySymbolLayout(ctx, int_symbol_type);
			auto const_int_layout      = querySymbolLayout(ctx, const_int_symbol_type);
			assertTrue(
				*int_layout == *const_int_layout, "Symbol mutability should not affect type layout"
			);

			testPrinting(int_layout, ctx);
			testPrinting(const_int_layout, ctx);
		});
	}

	void pointerLayoutManglingTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto i64 = SymbolType<>::withDefaults(
				getIntegralType(ctx, 64, compiler::tsh::IntegralAbstractType::Signedness::Signed)
			);
			auto ref_i64 = i64.withReferenceKind(ReferenceKind::Ref);
			auto box_i64 = i64.withReferenceKind(ReferenceKind::Box);

			auto i64_layout     = querySymbolLayout(ctx, i64);
			auto ref_i64_layout = querySymbolLayout(ctx, ref_i64);
			auto box_i64_layout = querySymbolLayout(ctx, box_i64);

			auto i64_name     = i64_layout->getMangledName();
			auto ref_i64_name = ref_i64_layout->getMangledName();
			auto box_i64_name = box_i64_layout->getMangledName();

			assertFalse(
				i64_name == ref_i64_name, "`i64 and `ref i64` layout should be mangled diferently."
			);
			assertFalse(
				ref_i64_name == box_i64_name,
				"`ref i64 and `box i64` layout should be mangled diferently."
			);
			assertFalse(
				box_i64_name == i64_name, "`box i64 and `i64` layout should be mangled diferently."
			);
		});
	}

public:
	~LowerTypeSystemSimpleTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/tsl/tests/")
