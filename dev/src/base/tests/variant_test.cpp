#include <base/extend_cpp/variant_match.hpp>

#include <tester/tester.hpp>

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
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(simpleTest); }

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
				assertTrue(v_c == 'b', "something went wrong");
				v_c = 'a';
				break;
				fail("break did nothing");
			}
			variant_default { fail("default happened"); }
		}

		assertTrue(std::get<char>(v) == 'a', "something went wrong");

		assertTrue(v_get(v, char) == 'a', "something went wrong");
		assertTrue(v_matches(v, char), "something went wrong");
		assertTrue(!v_matches(v, int, bool), "something went wrong");
		v_if_matches(v, bool, _) fail("if_v_matches");
		bool got_in = false;
		v_if_matches(v, char, _) { got_in = true; }
		assertTrue(got_in, "Should get in");

		bool default_ok = false;
		variant_match(v) {
			variant_case_novalue(int) { fail("bad variant access"); }
			variant_default { default_ok = true; }
		}

		assertTrue(default_ok, "Default did not happen");

		v = T();

		variant_match(v) {
			variant_case(T, v_t) { v_t.setData(2); }
			variant_default { fail("default happened (2)"); }
		}

		variant_match(v) {
			variant_case(T, v_t) { assertTrue(v_t.getData() == 2, "something failed"); }
			variant_default { fail("default happened (3)"); }
		}

		std::variant<int, double> v_2;
		v_2   = 2;
		i32 a = 3;

		VARIANT_VISIT(v_2, VISIT_CASE(auto&, any_v, {
						  any_v += 1;
						  any_v += a;
					  }));

		assertTrue(std::get<i32>(v_2) == 6, "bad variant access");

		VARIANT_VISIT(
			v_2,
			VISIT_CASE(
				int,
				i_v,
				{
					i_v = 100;
					assertTrue(i_v == 100, "something strange");
				}
			),
			VISIT_CASE([[maybe_unused]] auto&, any_v, { fail("Bad variant access"); })
		);

		assertTrue(std::get<i32>(v_2) == 6, "bad variant access");
	}
};

TESTER_COMMON_MAIN("/src/base/tests");
