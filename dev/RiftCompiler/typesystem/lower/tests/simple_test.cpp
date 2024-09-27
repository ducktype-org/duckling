#include <typesystem/lower/all.hpp>

#include <base/variant.hpp>
#include <tester/tester.hpp>
#include <typesystem/higher/queries.hpp>
#include <query_framework/test_utils/context_suite.hpp>
#include <query_framework/query_impl.hpp>

using namespace tsl;

class LowerTypeSystemSimpleTest final: public tester::ContextSuite {
#undef TESTER_CLASS
#define TESTER_CLASS LowerTypeSystemSimpleTest

public:
	LowerTypeSystemSimpleTest(tester::TestConfig&& config):
		  tester::ContextSuite(std::move(config), "Lower TypeSystem simple test") {
		TESTER_ADD_TEST(basic_types_test);
	}

private:
	void basic_types_test() {
		withContextDo([&](query::Context& ctx) -> void {
			tsh::UnitInfo unit_type   = ctx.query<tsh::QueryUnitType>({});
			TypeLayout    unit_layout = ctx.query<QueryTypeLayout>(unit_type);
			variant_match(unit_layout()) {
				variant_case(EmptyTypeLayout, l) {
					assertTrue(l.getSize() == 0, "Empty layout should have size zero.");
					assertTrue(
						l.getSourceType() == ctx.query<tsh::QueryUnitType>({}),
						"Layout should have source type as constructed."
					);
				}
				variant_default { fail("Layout of unit type should be empty."); }
			}

			tsh::TypeInfo byte_sized_types[]{
				ctx.query<tsh::QueryByteType>({}),
				ctx.query<tsh::QueryBoolType>({}),
				ctx.query<tsh::QueryCharType>({}),
			};
			for (tsh::TypeInfo byte_sized_type: byte_sized_types) {
				TypeLayout byte_sized_layout = ctx.query<QueryTypeLayout>(byte_sized_type);
				variant_match(byte_sized_layout()) {
					variant_case(IntegralTypeLayout, l) {
						assertTrue(
							l.getSize() == BYTE_SIZE,
							"Integral layout should have size equal to that of the source type."
						);
					}
					variant_default { fail("Layout of byte sized type should be integral."); }
				}
			}

			usize int_sizes[] = { 8, 16, 32, 64, 128 };
			for (int size: int_sizes) {
				TypeLayout int_layout
					= ctx.query<QueryTypeLayout>(ctx.query<tsh::QueryIntegralType>(size));
				variant_match(int_layout()) {
					variant_case(IntegralTypeLayout, l) {
						assertTrue(
							l.getSize() == size,
							"Integral layout should have size equal to that of the source type."
						);
					}
					variant_default { fail("Layout of integral type should be integral."); }
				}
			}

			usize float_sizes[] = { 16, 32, 64, 80, 128 };
			for (int size: float_sizes) {
				TypeLayout float_layout
					= ctx.query<QueryTypeLayout>(ctx.query<tsh::QueryFloatType>(size));
				variant_match(float_layout()) {
					variant_case(FloatTypeLayout, l) {
						assertTrue(
							l.getSize() == size,
							"Float layout should have size equal to that of the source type."
						);
					}
					variant_default { fail("Layout of float type should be float."); }
				}
			}

			TypeLayout functional_layout
				= ctx.query<QueryTypeLayout>(ctx.query<tsh::QueryFunctionType>({ {}, unit_type }));
			variant_match(functional_layout()) {
				variant_case(FunctionalTypeLayout, l) {
					assertTrue(
						l.getSize() == POINTER_SIZE,
						"Functional layout should have size equal to the size of a pointer."
					);
				}
				variant_default { fail("Layout of function type should be functional."); }
			}

			TypeLayout raw_pointer_layout
				= ctx.query<QueryTypeLayout>(ctx.query<tsh::QueryRawPointerType>({}));
			variant_match(raw_pointer_layout()) {
				variant_case(PointerTypeLayout, l) {
					assertTrue(
						l.getSize() == POINTER_SIZE,
						"Raw pointer layout should have size equal to the size of a pointer."
					);
					assertFalse(l.hasPointee(), "Raw pointer layout should not have pointee.");
				}
				variant_default { fail("Layout of raw pointer type should be pointer-like."); }
			}

			TypeLayout unit_pointer_layout
				= ctx.query<QueryTypeLayout>(ctx.query<tsh::QueryPointerType>({ unit_type }));
			variant_match(unit_pointer_layout()) {
				variant_case(PointerTypeLayout, l) {
					assertTrue(
						l.getSize() == POINTER_SIZE,
						"Raw pointer layout should have size equal to the size of a pointer."
					);
					assertTrue(l.hasPointee(), "Typed pointer layout should have pointee.");
					assertTrue(
						*l.getPointee() == unit_layout,
						"Pointee should be a layout of the pointed-to type."
					);
				}
				variant_default { fail("Layout of raw pointer type should be pointer-like."); }
			}
		});
	}

public:
	~LowerTypeSystemSimpleTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/typesystem/lower/tests/")
