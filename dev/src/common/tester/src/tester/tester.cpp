#include "tester.hpp"

#include <base/except/exceptions.hpp>
#include <base/except/pretty_stacktrace.hpp>

#include <cctype>
#include <chrono>
#include <ranges>
#include <string>
#include <unordered_set>

namespace tester {

	std::string addSpacesBeforeCapital(std::string_view str) {
		std::string result;
		result.reserve(str.size());
		bool was_capital = true;

		for (usize i = 0; i < str.size(); i++) {
			char c               = str[i];
			bool is_capital      = std::isupper(c);
			bool is_next_capital = i + 1 == str.size() or std::isupper(str[i + 1]);
			bool is_next_space   = i + 1 == str.size() or str[i + 1] == ' ';
			if (i > 0 and result.back() != ' ' and not is_next_space and is_capital
			    and not(was_capital and is_next_capital)) {
				result.push_back(' ');
			}
			was_capital = is_capital;
			result.push_back(c);
		}

		return result;
	}

	constexpr usize HEADER_LINE_LENGTH = 80;

	usize beginEqualSignL(usize name_l) {
		CORE_ASSERT(name_l <= HEADER_LINE_LENGTH, "Too long test name");
		return HEADER_LINE_LENGTH / 2 - (name_l / 2);
	}

	usize endEqualSignL(usize name_l) {
		CORE_ASSERT(name_l <= HEADER_LINE_LENGTH, "Too long test name");
		return HEADER_LINE_LENGTH / 2 - (name_l / 2) - (name_l % 2);
	}

	usize fullEqualSignL([[maybe_unused]] usize name_l) { return HEADER_LINE_LENGTH; }

	TestSuite::CritTestError::CritTestError(std::string_view message) {
		// This temporarily prints an early stack trace, since
		// we now often terminate, after CritTestError is thrown within the worker thread.
		std::cerr << "Critical test failure. Stopping test execution.\n";
		std::cerr << "Stack trace at the point of failure:\n";
		std::cerr << base::getCurrentStackTrace();
		std::cerr << "Error message: " << message << "\n";
	}

	const char* TestSuite::CritTestError::what() const noexcept {
		return "This shouldn't be called";
	}

	TestSuite::TestSuite(TestConfig&& config, std::string_view name):
		  curr_global_res(nullptr),
		  name(name),
		  config(std::move(config)) {}

	void TestSuite::assertTrue(bool v, std::string_view err, bool critical) {
		if (!v) fail(err, critical);
	}

	void TestSuite::assertFalse(bool v, std::string_view err, bool critical) {
		assertTrue(!v, err, critical);
	}

	void TestSuite::fail(std::string_view err, bool critical) {
		curr_global_res->success = false;
		message(err);
		if (critical) throw CritTestError(err);
	}

	void TestSuite::message(std::string_view mess) {
		std::string indent_mess = std::string("       ") + std::string(mess) + "\n";
		curr_global_res->output.push_back(printer::PrinterContent({
			indent_mess,
		}));
	}

	void TestSuite::filterTests(const std::vector<std::string>& tests_to_run) {
		if (tests_to_run.empty()) return;

		// Check for bad test names and report.
		for (const auto& requested_name: tests_to_run) {
			auto it = std::ranges::find(tests, requested_name, &TestData::name);
			if (it == tests.end()) {
				printer::StreamPrinter::print({ { "Warning: Test '", printer::Color::Yellow },
				                                { requested_name, printer::Color::Yellow },
				                                { "' not found and will be skipped.\n",
				                                  printer::Color::Yellow } });
			}
		}

		std::unordered_set<std::string> allowed(tests_to_run.begin(), tests_to_run.end());
		tests = tests | std::views::filter([&](const auto& t) { return allowed.contains(t.name); })
		      | std::ranges::to<std::vector>();
	}

	bool TestSuite::run() {
		prolog();
		usize passed = 0;
		usize failed = 0;

		auto begin = std::chrono::steady_clock::now();
		for (auto& t: tests) {
			TestResult res;
			curr_global_res = &res;

			runTest(t);
			if (t.should_fail) res.success = !res.success;
			resultHandler(t, res);

			if (curr_global_res->success) {
				passed++;
			} else {
				failed++;
				failed_tests.push_back(t.name);
			}

			if (curr_global_res->stop) break;
		}
		auto end     = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

		epilog(passed, failed, double(elapsed) / 1'000);

		return failed == 0;
	}

	void TestSuite::addTest(TestType test, std::string_view test_name, bool should_fail) {
		tests.push_back(TestData{
			.test = test, .name = std::string(test_name), .should_fail = should_fail });
	}

	void TestSuite::runTest(TestData test) {
		printer::StreamPrinter::print({ { "Running ", test.name, "\n" } });
		try {
			(this->*test.test)();
		} catch (const CritTestError& e) {
		} catch (const base::Panic& panic) {
			curr_global_res->success = false;
			message("Unexpected Panic:");
			message(panic.what());
		} catch (const base::LogicError& logic_error) {
			curr_global_res->success = false;
			message("Logic Error occurred:");
			message(logic_error.what());
		} catch (const base::NotYetImplemented& nyi) {
			curr_global_res->success = false;
			message("NotYetImplemented error:");
			message(nyi.what());
		} catch (const base::Exception& exception) {
			curr_global_res->success = false;
			message(
				"base::Exception was thrown. This was not expected. Add this exception to "
				"TestSuite."
			);
			message(exception.what());
		} catch (const std::exception& exception) {
			curr_global_res->success = false;
			message("std::exception was thrown. This was not expected.");
			message(exception.what());
		}
	}

	void TestSuite::resultHandler(const TestData& test, const TestResult& res) {
		printer::StreamPrinter::print({ {
			test.name,
			": ",
			res.success ? printer::PrinterContent("OK", printer::Color::Green)
						: printer::PrinterContent("FAIL", printer::Color::Red),
			"\n",
		} });
		for (auto& mess: res.output) printer::StreamPrinter::print(mess);
		if (res.output.empty()) printer::StreamPrinter::newline();
	}

	void TestSuite::prolog() {
		printer::StreamPrinter::print({ {
			std::string(beginEqualSignL(name.length() + 2), '='),
			" ",
			name,
			" ",
			std::string(endEqualSignL(name.length() + 2), '='),
			"\n",
			"Running ",
			std::to_string(tests.size()),
			" tests.\n",
		} });
		beforeAll();
	}

	void TestSuite::epilog(usize passed, usize failed, double time) {
		afterAll();
		stream_printer.print({ {
			"\n",
			std::string(fullEqualSignL(name.length() + 2), '='),
			"\n",
			"Elapsed time: ",
			std::to_string(time),
			" s",
			{ "\nPassed:       ", printer::Color::Green },
			{ std::to_string(passed), printer::Color::Green },
			{ "\nFailed:       ", failed ? printer::Color::Red : printer::Color::Default },
			{ std::to_string(failed), failed ? printer::Color::Red : printer::Color::Default },
			"\n",
			std::string(fullEqualSignL(name.length() + 2), '='),
			"\n",
		} });

		if (failed > 0) {
			stream_printer.print({ { "List of failed tests:\n", printer::Color::Red } });
			for (const auto& failed_name: failed_tests)
				stream_printer.print({ { "  - " + failed_name + "\n", printer::Color::Default } });

			stream_printer.print({ {
				std::string(fullEqualSignL(name.length() + 2), '='),
				"\n",
			} });
		}
	}
}
