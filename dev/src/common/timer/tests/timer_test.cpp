
#include <timer/timer.hpp>

#include <tester/tester.hpp>

class TimerTest final: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS TimerTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(zeroInitializationTest);
		TESTER_ADD_TEST(timeMeasurementTest);
		TESTER_ADD_TEST(printAsTest);
		TESTER_ADD_TEST(addToTimeTest);
	}

private:
	void zeroInitializationTest() {
		timer::Duration dur;
		assertTrue(
			dur.value == timer::Duration::DurationValueT::zero(),
			"Default-initialized Duration should be zero"
		);
	}

	void timeMeasurementTest() {
		timer::TimeMeasurement tm;
		tm.startMeasurement();
		for (int i = 0; i < 10'000; ++i)
			std::atomic_signal_fence(std::memory_order_seq_cst);  // just burn some time
		tm.endMeasurement();

		auto duration = tm.duration();
		assertTrue(duration.count() > 0, "Timer should measure some positive amount of time");

		tm.reset();
		tm.startMeasurement();
		tm.endMeasurement();
		auto duration2 = tm.duration();
		assertTrue(
			duration2.count() >= 0 and duration2.count() < duration.count() / 2,
			"Timer should measure some non-negative, small amount of time after reset"
		);


		assertThrows<base::Panic>(
			[&]() {
				timer::TimeMeasurement tm2;
				tm2.startMeasurement();
				tm2.startMeasurement();
			},
			"Starting measurement twice should panic"
		);

		assertThrows<base::Panic>(
			[&]() {
				timer::TimeMeasurement tm2;
				tm2.endMeasurement();
			},
			"Ending measurement without starting should panic"
		);

		assertThrows<base::Panic>(
			[&]() {
				timer::TimeMeasurement tm2;
				tm2.startMeasurement();
				tm2.endMeasurement();
				tm2.endMeasurement();
			},
			"Ending measurement twice should panic"
		);

		assertThrows<base::Panic>(
			[&]() {
				timer::TimeMeasurement tm2;
				tm2.startMeasurement();
				tm2.endMeasurement();
				tm2.startMeasurement();
			},
			"Starting measurement after ending should panic"
		);

		assertThrows<base::Panic>(
			[&]() {
				timer::TimeMeasurement tm2;
				auto                   _ = tm2.duration();
			},
			"Getting duration without measurement should panic"
		);
	}

	void printAsTest() {
		std::ostringstream out;

		timer::Duration dur_ns{ std::chrono::nanoseconds(1'500) };
		timer::printAs(out, dur_ns, timer::TimeUnit::Nanoseconds);
		assertTrue(out.str() == "1500 ns", "Printing 1500 ns should yield '1500 ns'");
		out.str("");

		timer::Duration dur_us{ std::chrono::microseconds(1'500) };
		timer::printAs(out, dur_us, timer::TimeUnit::Microseconds);
		assertTrue(out.str() == "1500 µs", "Printing 1500 µs should yield '1500 µs'");
		out.str("");

		timer::Duration dur_ms{ std::chrono::milliseconds(1'500) };
		timer::printAs(out, dur_ms, timer::TimeUnit::Milliseconds);
		assertTrue(out.str() == "1500 ms", "Printing 1500 ms should yield '1500 ms'");
		out.str("");

		timer::Duration dur_s{ std::chrono::seconds(3) };
		timer::printAs(out, dur_s, timer::TimeUnit::Seconds);
		assertTrue(out.str() == "3 s", "Printing 3 s should yield '3 s'");
		out.str("");
	}

	void addToTimeTest() {
		timer::Duration total_duration;
		ASSERT_EQUAL(total_duration.count(), 0);

		{
			timer::AddToTime add_to_time(&total_duration);
			for (int i = 0; i < 10'000; ++i)
				std::atomic_signal_fence(std::memory_order_seq_cst);  // just burn some time
		}

		assertTrue(total_duration.count() > 0, "AddToTime should add some positive amount of time");
	}
};


TESTER_COMMON_MAIN("/src/compiler/driver/tests/")
