#include <tester/tester.hpp>
#include <base/variant.hpp>

#include <variant>

class T {
	i32 data;

public:
	void setData(i32 v) { data = v; }

	i32 getData() { return data; }
};

class VariantUtilsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VariantUtilsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Variant Test") { TESTER_ADD_TEST(simpleTest); }

	void simpleTest() {
		std::variant<int, bool, char, T> v;
		v = 'b';
		variant_match(v) {
			variant_case_novalue(int) { fail("bad variant access"); }
			variant_case(bool, v_b) {
				fail("bad variant access");
				v_b = true;
			}
			variant_case(char, v_c) {
				assert(v_c == 'b', "something went wrong");
				v_c = 'a';
				break;
				fail("break did nothing");
			}
			variant_default { fail("default happened"); }
		}

		assert(std::get<char>(v) == 'a', "something went wrong");

		bool default_ok = false;
		variant_match(v) {
			variant_case_novalue(int) { fail("bad variant access"); }
			variant_default { default_ok = true; }
		}

		assert(default_ok, "Default did not happen");

		v = T();

		variant_match(v) {
			variant_case(T, v_t) { v_t.setData(2); }
			variant_default { fail("default happened (2)"); }
		}

		variant_match(v) {
			variant_case(T, v_t) { assert(v_t.getData() == 2, "something failed"); }
			variant_default { fail("default happened (3)"); }
		}

		std::variant<int, double> v_2;
		v_2   = 2;
		i32 a = 3;

		VARIANT_VISIT(v_2, VISIT_CASE(auto&, any_v, {
						  any_v += 1;
						  any_v += a;
					  }));

		assert(std::get<i32>(v_2) == 6, "bad variant access");

		VARIANT_VISIT(v_2, VISIT_CASE(int, i_v, {
						  i_v = 100;
						  assert(i_v == 100, "something strange");
					  }) VISIT_CASE([[maybe_unused]] auto&, any_v, {
						  fail("Bad variant access");
					  }));

		assert(std::get<i32>(v_2) == 6, "bad variant access");
	}
};

TESTER_COMMON_MAIN("base/tests");
