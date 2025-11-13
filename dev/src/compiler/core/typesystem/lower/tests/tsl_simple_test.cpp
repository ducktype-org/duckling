#include <helios/test_utils/helios_test_utils.hpp>
#include <typesystem/higher/queries.hpp>
#include <typesystem/higher/type_interface.hpp>
#include <typesystem/lower/all.hpp>

#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/context.hpp>
#include <query_framework/utils/with_context_do.hpp>
#include <tester/tester.hpp>

using namespace tsl;
using namespace tsh;
using query::utils::withContextDo;

class LowerTypeSystemSimpleTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LowerTypeSystemSimpleTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(basicTypesTest);
		TESTER_ADD_TEST(stringTest);
		TESTER_ADD_TEST(dynamicArrayTest);
		TESTER_ADD_TEST(variantTest);
		TESTER_ADD_TEST(tupleTest);
		TESTER_ADD_TEST(classTest);
		TESTER_ADD_TEST(mutabilityTest);
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
			const UnitAbstractType unit_type   = ctx.query<QueryUnitType>({});
			auto                   unit_layout = ctx.query<QueryAbstractTypeLayout>(unit_type);

			assertTrue(unit_layout->getSize() == Bits(0), "Empty layout should have size zero.");
			assertTrue(
				unit_layout->getSourceType() == ctx.query<QueryUnitType>({}),
				"Layout should have source type as constructed."
			);
			variant_match(unit_layout->getVariant()) {
				variant_case(EmptyTypeLayout, l) { /* good */ }
				variant_default { fail("Layout of unit type should be empty."); }
			}
			testPrinting(unit_layout, ctx);

			const std::array<std::pair<AbstractType, Bits>, 3> small_types{ {
				{ ctx.query<QueryBoolType>({}), BOOL_SIZE },
				{ ctx.query<QueryByteType>({}), BYTE_SIZE },
				{ ctx.query<QueryCharType>({}), CHAR_SIZE },
			} };
			for (auto [small_type, expected_small_size]: small_types) {
				auto small_layout = ctx.query<QueryAbstractTypeLayout>(small_type);
				assertTrue(
					small_layout->getSize() == expected_small_size,
					"Integral layout should have size equal to that of the source type."
				);
				assertTrue(
					small_layout->getSourceType() == small_type,
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
				IntegralAbstractType int_type   = ctx.query<QueryIntegralType>(size);
				auto                 int_layout = ctx.query<QueryAbstractTypeLayout>(int_type);
				assertTrue(
					int_layout->getSize() == Bits(size),
					"Integral layout should have size equal to that of the source type."
				);
				assertTrue(
					int_layout->getSourceType() == int_type,
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
				FloatAbstractType float_type   = ctx.query<QueryFloatType>(size);
				auto              float_layout = ctx.query<QueryAbstractTypeLayout>(float_type);
				assertTrue(
					float_layout->getSize() == Bits(size),
					"Float layout should have size equal to that of the source type."
				);
				assertTrue(
					float_layout->getSourceType() == float_type,
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
			auto functional_layout = ctx.query<QueryAbstractTypeLayout>(function_type);
			assertTrue(
				functional_layout->getSize() == POINTER_SIZE,
				"Functional layout should have size equal to the size of a pointer."
			);
			assertTrue(
				functional_layout->getSourceType() == function_type,
				"Layout should have source type as constructed."
			);
			variant_match(functional_layout->getVariant()) {
				variant_case(FunctionalTypeLayout, l) { /* good */ }
				variant_default { fail("Layout of function type should be functional."); }
			}
			testPrinting(functional_layout, ctx);

			const RawPointerAbstractType raw_pointer_type = ctx.query<QueryRawPointerType>({});
			auto raw_pointer_layout = ctx.query<QueryAbstractTypeLayout>(raw_pointer_type);
			assertTrue(
				raw_pointer_layout->getSize() == POINTER_SIZE,
				"Raw pointer layout should have size equal to the size of a pointer."
			);
			assertTrue(
				raw_pointer_layout->getSourceType() == raw_pointer_type,
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
			auto unit_pointer_layout = ctx.query<QueryAbstractTypeLayout>(unit_pointer_type);
			assertTrue(
				unit_pointer_layout->getSize() == POINTER_SIZE,
				"Typed pointer layout should have size equal to the size of a pointer."
			);
			assertTrue(
				unit_pointer_layout->getSourceType() == unit_pointer_type,
				"Layout should have source type as constructed."
			);
			variant_match(unit_pointer_layout->getVariant()) {
				variant_case(PointerTypeLayout, l) {
					assertTrue(l.hasPointee(), "Typed pointer layout should have pointee.");
					assertTrue(
						l.getPointee() == unit_layout,
						"Pointee should be a layout of the pointed-to type."
					);
				}
				variant_default { fail("Layout of raw pointer type should be pointer-like."); }
			}
			testPrinting(unit_pointer_layout, ctx);
		});
	}

	void stringTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const StringAbstractType string_type = ctx.query<QueryStringType>({});
			auto string_layout                   = ctx.query<QueryAbstractTypeLayout>(string_type);

			assertTrue(
				string_layout->getSourceType() == string_type,
				"Layout should have source type as constructed."
			);
			variant_match(string_layout->getVariant()) {
				variant_case(StringTypeLayout, l) { /* good */ }
				variant_default { fail("Layout of string type should be string-like."); }
			}
			testPrinting(string_layout, ctx, true);
		});
	}

	void dynamicArrayTest() {
		withContextDo([&](query::Context& ctx) -> void {
			const UnitAbstractType unit_type   = ctx.query<QueryUnitType>({});
			auto                   unit_layout = ctx.query<QueryAbstractTypeLayout>(unit_type);

			const DynamicArrayAbstractType dynamic_array_type
				= ctx.query<QueryDynamicArrayType>(st(unit_type));
			auto dynamic_array_layout = ctx.query<QueryAbstractTypeLayout>(dynamic_array_type);

			assertTrue(
				dynamic_array_layout->getSourceType() == dynamic_array_type,
				"Layout should have source type as constructed."
			);

			testPrinting(dynamic_array_layout, ctx, true);
		});
	}

	void variantTest() {
		withContextDo([&](query::Context& ctx) -> void {
			SymbolType<>              i8_type  = st(ctx.query<QueryIntegralType>({ 8 }));
			SymbolType<>              f16_type = st(ctx.query<QueryFloatType>(16));
			const VariantAbstractType variant_type
				= ctx.query<QueryVariantType>({ { i8_type, f16_type } });
			auto variant_layout = ctx.query<QueryAbstractTypeLayout>(variant_type);

			assertTrue(
				variant_layout->getSize() == BYTE_SIZE * 2 + Bits(16),
				"Variant layout size should account for data alignment."
			);
			assertTrue(
				variant_layout->getSourceType() == variant_type,
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
						l.getTypeOfIndex(0) != l.getTypeOfIndex(1),
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
			const SymbolType<> i8_type  = st(ctx.query<QueryIntegralType>({ 8 }));
			const SymbolType<> f16_type = st(ctx.query<QueryFloatType>(16));
			const SymbolType<> f64_type = st(ctx.query<QueryFloatType>(64));
			const SymbolType<> f16_ref
				= SymbolType<>{ ctx.query<QueryFloatType>(16), ReferenceKind::Ref, Immutable };
			const SymbolType<> f16_box
				= SymbolType<>{ ctx.query<QueryFloatType>(16), ReferenceKind::Box, Immutable };
			const TupleAbstractType tuple_type   = ctx.query<QueryTupleType>({
                { i8_type, f16_type, f64_type, f16_ref, f16_box },
            });
			auto                    tuple_layout = ctx.query<QueryAbstractTypeLayout>(tuple_type);

			assertTrue(
				tuple_layout->getSize() == BYTE_SIZE * 16 + POINTER_SIZE * 2,
				"Tuple layout size should account for data alignment and references."
			);
			assertTrue(
				tuple_layout->getSourceType() == tuple_type,
				"Layout should have source type as constructed."
			);
			variant_match(tuple_layout->getVariant()) {
				variant_case(TupleTypeLayout, l) {
					assertTrue(
						l.getComponentOffset(0) == Bytes(0) && l.getComponentOffset(1) == Bytes(2)
							&& l.getComponentOffset(2) == Bytes(8),
						"Tuple layout should align its component layouts."
					);
				}
				variant_default { fail("Layout of tuple type should be tuple-like."); }
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
			TypeInterface           my_class_interface = my_class_type.getInterface(ctx);

			const SymID a_field_symbol = [&] {
				variant_match(my_class_interface.resolve(base::StrID("a"), ctx)) {
					variant_case(TypeInterface::SingleMatch, m) { return m.best_match.getSymbol(); }
				}
				CORE_PANIC("Could not resolve field.");
			}();
			const SymID b_field_symbol = [&] {
				variant_match(my_class_interface.resolve(base::StrID("b"), ctx)) {
					variant_case(TypeInterface::SingleMatch, m) { return m.best_match.getSymbol(); }
				}
				CORE_PANIC("Could not resolve field.");
			}();
			const SymID c_field_symbol = [&] {
				variant_match(my_class_interface.resolve(base::StrID("c"), ctx)) {
					variant_case(TypeInterface::SingleMatch, m) { return m.best_match.getSymbol(); }
				}
				CORE_PANIC("Could not resolve field.");
			}();
			const SymID d_field_symbol = [&] {
				variant_match(my_class_interface.resolve(base::StrID("d"), ctx)) {
					variant_case(TypeInterface::SingleMatch, m) { return m.best_match.getSymbol(); }
				}
				CORE_PANIC("Could not resolve field.");
			}();
			const SymID e_field_symbol = [&] {
				variant_match(my_class_interface.resolve(base::StrID("e"), ctx)) {
					variant_case(TypeInterface::SingleMatch, m) { return m.best_match.getSymbol(); }
				}
				CORE_PANIC("Could not resolve field.");
			}();

			auto my_class_layout = ctx.query<QueryAbstractTypeLayout>(my_class_type);
			assertTrue(
				// (1 + padding 1) + (2 + padding 2) + 8 + 8 + 8
				my_class_layout->getSize() == BYTE_SIZE * 32,
				"Class layout size should account for data alignment."
			);
			assertTrue(
				my_class_layout->getSourceType() == my_class_type,
				"Layout should have source type as constructed."
			);

			variant_match(my_class_layout->getVariant()) {
				variant_case(ClassTypeLayout, l) {
					assertTrue(
						l.getFieldOffset(a_field_symbol) == Bytes(0)
							&& l.getFieldOffset(b_field_symbol) == Bytes(2)
							&& l.getFieldOffset(c_field_symbol) == Bytes(8)
							&& l.getFieldOffset(d_field_symbol) == Bytes(16)
							&& l.getFieldOffset(e_field_symbol) == Bytes(24),
						"Class layout should align its component layouts."
					);
				}
				variant_default { fail("Layout of class type should be class-like."); }
			}
			testPrinting(my_class_layout, ctx, true);
		});
	}

	void mutabilityTest() {
		withContextDo([&](query::Context& ctx) -> void {
			auto int_symbol_type       = st(ctx.query<QueryIntegralType>(64));
			auto const_int_symbol_type = int_symbol_type.withMutability(Immutable);
			auto int_layout            = ctx.query<QuerySymbolTypeLayout>(int_symbol_type);
			auto const_int_layout      = ctx.query<QuerySymbolTypeLayout>(const_int_symbol_type);
			assertTrue(
				int_layout == const_int_layout, "Symbol mutability should not affect type layout"
			);

			testPrinting(int_layout, ctx);
			testPrinting(const_int_layout, ctx);
		});
	}

public:
	~LowerTypeSystemSimpleTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/typesystem/lower/tests/")
