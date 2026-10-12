// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include <helios/tsh/abstract_type.hpp>
#include <helios/tsh/queries/types.hpp>

#include <query_framework/context/context.hpp>
#include <query_framework/entry/query_entry_point.hpp>
#include <query_framework/entry/with_context_do.hpp>
#include <tester/tester.hpp>

#include <any>
#include <sstream>

using namespace compiler::tsh;

class HigherTypeSystemErrorTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS HigherTypeSystemErrorTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(integralSizeErrorTest);
		TESTER_ADD_TEST(floatSizeErrorTest);
	}

private:
	/**
	 * WithContextCompute helper wrapper to avoid boilerplate.
	 */
	auto getIntegralTypeNoContext(
		u64 size, compiler::tsh::IntegralAbstractType::Signedness signedness
	) {
		return std::any_cast<compiler::tsh::IntegralAbstractType>(
			query::utils::withContextCompute([&](query::Context& ctx) {
				return compiler::tsh::getIntegralType(ctx, size, signedness);
			})
		);
	}

	/**
	 * WithContextCompute helper wrapper to avoid boilerplate.
	 */
	auto getFloatTypeNoContext(u64 size) {
		return std::any_cast<compiler::tsh::FloatAbstractType>(query::utils::withContextCompute(
			[&](query::Context& ctx) { return compiler::tsh::getFloatType(ctx, size); }
		));
	}

	void integralSizeErrorTest() {
		// this should log an error since 42 is not a valid integral size:
		getIntegralTypeNoContext(42, compiler::tsh::IntegralAbstractType::Signedness::Signed);

		std::stringstream dumped_logs;
		auto              logger = query::Context::dumpToOneLoggerAndClear();
		assertTrue(logger->bad(), "Requesting bad integral size should result in an error.");
		logger->dumpLog(false, dumped_logs);
		const auto dumped_logs_str = dumped_logs.str();
		assertTrue(
			dumped_logs_str.find("Invalid size of integral type") != decltype(dumped_logs_str)::npos,
			"Logs should contain mention of invalid integral size."
		);
	}

	void floatSizeErrorTest() {
		// this should log an error since 42 is not a valid float size:
		getFloatTypeNoContext(42);

		std::stringstream dumped_logs;
		auto              logger = query::Context::dumpToOneLoggerAndClear();
		assertTrue(logger->bad(), "Requesting bad float size should result in an error.");
		logger->dumpLog(false, dumped_logs);
		const auto dumped_logs_str = dumped_logs.str();
		assertTrue(
			dumped_logs_str.find("Invalid size of float type") != decltype(dumped_logs_str)::npos,
			"Logs should contain mention of invalid float size."
		);
	}

public:
	~HigherTypeSystemErrorTest() override = default;
};

TESTER_COMMON_MAIN("/src/compiler/core/helios/tests/")
