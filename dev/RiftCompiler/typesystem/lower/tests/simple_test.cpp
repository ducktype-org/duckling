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
		TESTER_ADD_TEST(do_test);
	}

private:
	void do_test() {
		withContextDo([&](query::Context& ctx) -> void {
			TypeLayout int32_layout
				= ctx.query<QueryTypeLayout>(ctx.query<tsh::QueryIntegralType>(32));

			variant_match(int32_layout()) {
				variant_case(IntegralTypeLayout, itl) {
					assertTrue(itl.getSize() == 32, "Integral layout should have size 32.");
				}
				variant_default {
					fail("Layout of integral type should be integral.");
				}
			}
		});
	}

public:
	~LowerTypeSystemSimpleTest() override = default;
};

TESTER_COMMON_MAIN("/RiftCompiler/typesystem/lower/tests/")
