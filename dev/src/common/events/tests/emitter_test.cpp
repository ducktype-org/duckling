#include <events/emitter.hpp>

#include <tester/tester.hpp>

class SimpleEventsTest: public tester::TestSuite {
#undef TESTER_CLASS
#define TESTER_CLASS SimpleEventsTest

public:
	TESTER_TEST_SIMPLE_CONSTRUCTOR() {
		TESTER_ADD_TEST(simpleTest);
		TESTER_ADD_TEST(detachTest);
		TESTER_ADD_TEST(multipleListenersTest);
		TESTER_ADD_TEST(multipleEmittersTest);
		TESTER_ADD_TEST(selfDetachTest);
	}

private:
	struct IntEvent {
		int value;
	};

	void simpleTest() {
		events::Emitter<IntEvent> emitter;

		int                        some_number = 7;
		events::Listener<IntEvent> listener([&](IntEvent event) { some_number = event.value; });

		emitter.attachListener(listener);

		emitter.emitEvent({ 42 });

		ASSERT_EQUAL_PRINT(42, some_number);
	}

	void detachTest() {
		events::Emitter<IntEvent> emitter;

		int                        some_number = 7;
		events::Listener<IntEvent> listener([&](IntEvent event) { some_number = event.value; });

		emitter.attachListener(listener);

		emitter.emitEvent({ 8 });
		ASSERT_EQUAL_PRINT(8, some_number);

		emitter.detachListener(listener);

		emitter.emitEvent({ 42 });
		ASSERT_EQUAL_PRINT(8, some_number);
	}

	void multipleListenersTest() {
		events::Emitter<IntEvent> emitter;

		int some_number  = 7;
		int other_number = 8;
		events::Listener<IntEvent> some_listener([&](IntEvent event) { some_number = event.value; });
		events::Listener<IntEvent> other_listener([&](IntEvent event) {
			other_number = event.value;
		});

		emitter.attachListener(some_listener);
		emitter.attachListener(other_listener);

		emitter.emitEvent({ 42 });

		ASSERT_EQUAL_PRINT(42, some_number);
		ASSERT_EQUAL_PRINT(42, other_number);
	}

	void multipleEmittersTest() {
		events::Emitter<IntEvent> emitter1;
		events::Emitter<IntEvent> emitter2;

		int                        some_number = 7;
		events::Listener<IntEvent> listener([&](IntEvent event) { some_number = event.value; });

		emitter1.attachListener(listener);

		emitter1.emitEvent({ 42 });
		ASSERT_EQUAL_PRINT(42, some_number);

		assertThrows<std::runtime_error>(
			[&]() { emitter2.attachListener(listener); }, "Reatach should have thrown"
		);

		listener.detach();

		emitter2.attachListener(listener);

		emitter2.emitEvent({ 8 });
		ASSERT_EQUAL_PRINT(8, some_number);

		emitter1.emitEvent({ 42 });
		ASSERT_EQUAL_PRINT(8, some_number);
	}

	void selfDetachTest() {
		events::Emitter<IntEvent> emitter;

		int some_number = 7;

		events::Listener<IntEvent> listener([&](IntEvent event) {
			some_number = event.value;
			emitter.detachListener(listener);
		});

		emitter.attachListener(listener);

		emitter.emitEvent({ 42 });

		ASSERT_EQUAL_PRINT(42, some_number);

		emitter.emitEvent({ 8 });

		ASSERT_EQUAL_PRINT(42, some_number);
	}
};

TESTER_COMMON_MAIN("/src/common/events/tests/");
