#include <tester/tester.hpp>
#include <base/variant.hpp>

#include <variant>

class T {
	int data;
public:
	void setData(int v) { data = v; }
	int getData() { return data; }
};

class VariantUtilsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS VariantUtilsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR("Variant Test") {
		TESTER_ADD_TEST(simpleTest);
	}

	void simpleTest() {
		std::variant<int, bool, char, T> v;
		v = 'b';
		VARIANT_MATCH(v,
			VARIANT_CASE(int, v_i, {
				fail("bad variant access"); 
				v_i = 2;
			})
			VARIANT_CASE(bool, v_b, {
				fail("bad variant access");
				v_b = 2;
			})
			VARIANT_CASE(char, v_c, {
				assert(v_c == 'b', "something went wrong");
				v_c = 'a';
				break;
				fail("break did nothing");
			})
		)

		assert(std::get<char>(v) == 'a', "something went wrong");

		VARIANT_MATCH(v,
			VARIANT_CASE(T, v_t, {
				v_t.setData(2);
			})
		)
		VARIANT_MATCH(v,
			VARIANT_CASE(T, v_t, {
				assert(v_t.getData() == 2, "something failed");
			})
		)


		std::variant<int, double> v_2;
		v_2 = 2;
		int a = 3;
		
		VARIANT_VISIT(v_2,
			VISIT_CASE(auto&, any_v, {
				any_v += 1;
				any_v += a;
			})
		);

		assert(std::get<int>(v_2) == 6, "bad variant access");
		
		VARIANT_VISIT(v_2,
			VISIT_CASE(int, i_v, {
				i_v = 100;
				assert(i_v == 100, "something strange");
			})
			VISIT_CASE([[maybe_unused]]auto&, any_v, {
				fail("Bad variant access");
			})
		);

		assert(std::get<int>(v_2) == 6, "bad variant access");
	}

};

TESTER_COMMON_MAIN("/base/tests");

