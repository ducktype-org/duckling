#include <tester/tester.hpp>
#include <helios/tsh/queries/types.hpp>
#include <ctv/ctv.hpp>
#include <query_framework/entry/with_context_do.hpp>

class CTVTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CTVTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(queryHashTest);
	}

	~CTVTest() override = default;

private:

	void queryHashTest() {
		using namespace compiler::ctv;

		std::vector<CompileTimeValue> unique_values;

		query::utils::withContextDo([&](query::Context& ctx) {			
			unique_values.emplace_back(true);
			unique_values.emplace_back(false);
			
			unique_values.emplace_back(NumericValue::createOfType(compiler::tsh::getIntegralType(ctx, 32, compiler::tsh::IntegralAbstractType::Signedness::Signed), 12).value());
			
			unique_values.emplace_back('a');
			unique_values.emplace_back('b');
			unique_values.emplace_back('\0');
			unique_values.emplace_back('d');
			unique_values.emplace_back(' ');
		});

		std::set<base::Bit256> hashes;
		for (const auto& value: unique_values) {
			const base::Bit256 hash = value.queryUnstablePerfectHash();
			assertTrue(hashes.find(hash) == hashes.end(), base::strConcat("Hash collision detected for value: ", value.toString()));
			hashes.insert(hash);
		}

	}

};

TESTER_COMMON_MAIN("/src/compiler/core/ctv/tests/");
