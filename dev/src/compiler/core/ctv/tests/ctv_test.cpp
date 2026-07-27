#include <ctv/ctv.hpp>
#include <helios/tsh/queries/types.hpp>

#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

class CTVTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS CTVTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() { TESTER_ADD_TEST(queryHashTest); }

	~CTVTest() override = default;

private:
	void queryHashTest() {
		using namespace compiler::ctv;

		std::vector<CompileTimeValue> unique_values;

		query::utils::withContextDo([&](query::Context& ctx) {
			unique_values.emplace_back(true);
			unique_values.emplace_back(false);

			for (i64 val = -10; val <= 80; val++) {
				for (u64 size: { 8u, 16u, 32u, 64u }) {
					unique_values.emplace_back(
						NumericValue::createOfType(
							compiler::tsh::getIntegralType(
								ctx, size, compiler::tsh::IntegralAbstractType::Signedness::Signed
							),
							val
						)
							.value()
					);

					if (val >= 0) {
						unique_values.emplace_back(
							NumericValue::createOfType(
								compiler::tsh::getIntegralType(
									ctx,
									size,
									compiler::tsh::IntegralAbstractType::Signedness::Unsigned
								),
								val
							)
								.value()
						);
					}
				}
			}

			unique_values.emplace_back('a');
			unique_values.emplace_back('b');
			unique_values.emplace_back('\0');
			unique_values.emplace_back('d');
			unique_values.emplace_back(' ');

			unique_values.emplace_back(CompileTimeValue::UnitCTV{});
			unique_values.emplace_back(CompileTimeValue::CharSliceValue{ base::StrID("test_string_1") });
			unique_values.emplace_back(CompileTimeValue::CharSliceValue{ base::StrID("a") });
			unique_values.emplace_back(CompileTimeValue::CharSliceValue{ base::StrID("b") });
			unique_values.emplace_back(CompileTimeValue::CharSliceValue{ base::StrID("d") });
			unique_values.emplace_back(CompileTimeValue::CharSliceValue{ base::StrID(" ") });
		});

		std::set<base::Bit256> hashes;
		for (const auto& value: unique_values) {
			const base::Bit256 hash = value.queryUnstablePerfectHash();
			assertTrue(
				hashes.find(hash) == hashes.end(),
				base::strConcat("Hash collision detected for value: ", value.toString())
			);
			hashes.insert(hash);
		}
	}
};

TESTER_COMMON_MAIN("/src/compiler/core/ctv/tests/");
