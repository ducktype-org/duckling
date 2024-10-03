#include "tester.hpp"
#include <base/exceptions.hpp>
#include <chrono>
#include <cctype>

namespace tester {

	std::string addSpacesBeforeCapital(std::string_view view) {
		std::string result;
		result.reserve(view.size());

		for (usize i = 0; i < view.size(); ++i) {
			if (i != 0 and std::isupper(view[i])) result.push_back(' ');
			result.push_back(view[i]);
		}

		return result;
	}

	constexpr usize header_line_length = 40;

	usize beginEqualSignL(usize name_l) { return header_line_length / 2 - (name_l / 2); }

	usize endEqualSignL(usize name_l) {
		return header_line_length / 2 - (name_l / 2) - (name_l % 2);
	}

	usize fullEqualSignL([[maybe_unused]] usize name_l) { return header_line_length; }

	const char* TestSuite::CritTestError::what() const noexcept {
		return "This shouldn't be called";
	}

	TestSuite::TestSuite(TestConfig&& config, std::string_view name):
		  curr_global_res(nullptr),
		  name(name),
		  config(std::move(config)) {}

	void TestSuite::assertTrue(bool v, std::string_view err, bool critical) {
		if (!v) {
			curr_global_res->success = false;
			message(err);
			if (critical) throw CritTestError();
		}
	}

	void TestSuite::assertFalse(bool v, std::string_view err, bool critical) {
		assertTrue(!v, err, critical);
	}

	void TestSuite::fail(std::string_view err) {
		curr_global_res->success = false;
		message(err);
		throw CritTestError();
	}

	void TestSuite::message(std::string_view mess) {
		std::string indent_mess = std::string("       ") + std::string(mess) + "\n";
		curr_global_res->output.push_back(printer::PrinterContent({
			indent_mess,
		}));
	}

	bool TestSuite::run() {
		prolog();
		usize passed = 0;
		usize failed = 0;

		auto begin = std::chrono::steady_clock::now();
		for (auto& t: tests) {
			TestResult res;
			curr_global_res = &res;

			runTest(t.test);
			resultHandler(t, res);

			passed += curr_global_res->success;
			failed += !curr_global_res->success;
			if (curr_global_res->stop) break;
		}
		auto end     = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

		epilog(passed, failed, double(elapsed) / 1'000);

		return failed == 0 && passed == tests.size();
	}

	void TestSuite::addTest(TestType test, std::string_view test_name) {
		tests.push_back(TestData{ test, std::string(test_name) });
	}

	void TestSuite::runTest(TestType test) {
		try {
			(this->*test)();
		} catch (const CritTestError& e) {
		} catch (const base::Panic& panic) {
			curr_global_res->success = false;
			message("Unexpected Panic occurred in:");
			message(panic.getPosition());
			message("Error:");
			message(panic.what());
		} catch (const base::LogicError& logicError) {
			curr_global_res->success = false;
			message("Logic Error occurred:");
			message(logicError.what());
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
			res.success ? printer::PrinterContent("OK", printer::Color::GREEN)
						: printer::PrinterContent("FAIL", printer::Color::RED),
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
	}

	void TestSuite::epilog(usize passed, usize failed, double time) {
		streamPrinter.print({ {
			"\n",
			std::string(fullEqualSignL(name.length() + 2), '='),
			"\n",
			"Elapsed time: ",
			std::to_string(time),
			" s",
			{ "\nPassed:       ", printer::Color::GREEN },
			{ std::to_string(passed), printer::Color::GREEN },
			{ "\nFailed:       ", failed ? printer::Color::RED : printer::Color::DEFAULT },
			{ std::to_string(failed), failed ? printer::Color::RED : printer::Color::DEFAULT },
			"\n",
			std::string(fullEqualSignL(name.length() + 2), '='),
			"\n",
		} });
	}
}
