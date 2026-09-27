#pragma once

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>

#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace vm::test {
	class ExpectedValue;

	/**
	 * @brief Expected shape of a value returned by an expression, mirroring the members of
	 * `InterpretedDataVariant`. Defined outside `ExpectedValue` so the recursive alternatives can
	 * refer back to it (collections and the pointee use indirection).
	 */
	struct Primitive final {
		u64 value;
	};

	struct Ptr final {
		/// Empty checks only that the pointer is non-null; set checks the pointee recursively.
		std::shared_ptr<ExpectedValue> pointee;
	};

	struct Struct final {
		std::vector<std::pair<base::StrID, ExpectedValue>> fields;
	};

	struct Variant final {
		u64                            type_tag;
		std::shared_ptr<ExpectedValue> inner;
	};

	struct Table final {
		std::vector<ExpectedValue> elements;
	};

	class ExpectedValue final {
		std::variant<Primitive, Ptr, Struct, Variant, Table> value;

	public:
		ExpectedValue(Primitive expected): value(expected) {}

		ExpectedValue(Ptr expected): value(std::move(expected)) {}

		ExpectedValue(Struct expected): value(std::move(expected)) {}

		ExpectedValue(Variant expected): value(std::move(expected)) {}

		ExpectedValue(Table expected): value(std::move(expected)) {}

		ExpectedValue(u64 primitive): value(Primitive{ primitive }) {}

		static ExpectedValue primitive(u64 value) { return Primitive{ value }; }

		static ExpectedValue nonNullPtr() { return Ptr{}; }

		static ExpectedValue ptrTo(ExpectedValue pointee) {
			return Ptr{ std::make_shared<ExpectedValue>(std::move(pointee)) };
		}

		static ExpectedValue structure(std::vector<std::pair<base::StrID, ExpectedValue>> fields) {
			return Struct{ std::move(fields) };
		}

		static ExpectedValue variant(u64 type_tag, ExpectedValue inner) {
			return Variant{
				.type_tag = type_tag,
				.inner    = std::make_shared<ExpectedValue>(std::move(inner)),
			};
		}

		static ExpectedValue variant(u64 type_tag) {
			return Variant{
				.type_tag = type_tag,
				.inner    = nullptr,
			};
		}

		static ExpectedValue table(std::vector<ExpectedValue> elements) {
			return Table{ std::move(elements) };
		}

		[[nodiscard]] const auto& get() const { return value; }
	};

	class FlowSimulator {
		/// Upper bound for waiting on an expression result, in milliseconds.
		static constexpr u64 DEFAULT_EXPR_TIMEOUT = 500;

		/// A pending expression together with the position it was called from. `ret_from_expr`
		/// returns the thread to that exact position, so the result can enforce it on completion.
		struct PendingExpr {
			api::ExprResult future;
			base::StrID     caller_function;
			u64             caller_instr;
		};

		/// Expressions awaiting their result. LIFO: the most recent expression completes first.
		std::deque<PendingExpr> pending_expr_results;

		PendingExpr popPendingExpr() {
			CORE_ASSERT(!pending_expr_results.empty(), "There is no pending expression to await");
			auto pending = std::move(pending_expr_results.back());
			pending_expr_results.pop_back();
			return pending;
		}

	public:
		FlowSimulator(
			std::function<void(bool, std::string_view)> assert_true_fn,
			vm::PID                                     pid,
			vm::api::ThreadID                           thread_id = vm::api::ThreadID(0)
		):
			  assert_true_fn(std::move(assert_true_fn)),
			  pid(pid),
			  thread_id(thread_id) {}

		FlowSimulator& putBreakpoint(base::StrID func_name, u64 instr_index) {
			auto bp_res = vm::api::setBreakpoint(pid, func_name, instr_index, true);
			if (!bp_res) assertTrue(false, vm::api::errorToString(bp_res.error()));
			return *this;
		}

		FlowSimulator& disableBreakpoint(base::StrID func_name, u64 instr_index) {
			auto bp_res = vm::api::setBreakpoint(pid, func_name, instr_index, false);
			if (!bp_res) assertTrue(false, vm::api::errorToString(bp_res.error()));
			return *this;
		}

		FlowSimulator& runMain() {
			auto run_res = vm::api::run(pid);
			if (!run_res)
				assertTrue(
					false, base::strConcat("Run failed: ", vm::api::errorToString(run_res.error()))
				);
			return *this;
		}

		/// Waits for the next pause and asserts it happened at the expected position. Prefer this
		/// overload; use the position-less one only when the location is genuinely unknown.
		FlowSimulator& awaitBreakpoint(base::StrID expected_func, u64 expected_instr) {
			auto bp_res = vm::api::waitForBreakpoint(pid, thread_id);
			if (!bp_res)
				assertTrue(
					false,
					base::strConcat(
						"Wait for breakpoint failed: ", vm::api::errorToString(bp_res.error())
					)
				);
			assertEqual(expected_func, bp_res->function_name, "Breakpoint function mismatch");
			assertEqual(expected_instr, bp_res->instr_number, "Breakpoint instruction mismatch");

			return *this;
		}

		/// Waits for the next pause without checking where it happened. Use only when the expected
		/// position cannot be known (e.g. stepping into an infinite loop).
		FlowSimulator& awaitBreakpoint() {
			auto response = vm::api::waitForBreakpoint(pid, thread_id);
			if (!response)
				assertTrue(
					false,
					base::strConcat(
						"Wait for pause failed: ", vm::api::errorToString(response.error())
					)
				);
			return *this;
		}

		using FrameVar         = std::pair<base::StrID, base::StrID>;
		using FrameVars        = std::vector<FrameVar>;
		using FrameExpectation = std::pair<base::StrID, base::Optional<FrameVars>>;

		FlowSimulator& enforceCallStack(const std::vector<FrameExpectation>& expected) {
			auto frames = vm::api::debuggerGetNumberOfStackFrames(pid, thread_id);
			if (!frames) assertTrue(false, vm::api::errorToString(frames.error()));

			assertEqual(
				expected.size(), usize(frames->number_of_stack_frames), "Call stack size mismatch"
			);

			for (u64 frame_index = 0; frame_index < frames->number_of_stack_frames; frame_index++) {
				auto data = vm::api::debuggerGetStackFrameData(pid, thread_id, frame_index);
				if (!data) assertTrue(false, vm::api::errorToString(data.error()));

				const auto& [expected_name, expected_vars] = expected.at(usize(frame_index));

				assertEqual(
					expected_name,
					data->function_name,
					base::strConcat("Call stack mismatch at frame ", frame_index)
				);

				if (!expected_vars) continue;

				assertEqual(
					expected_vars->size(),
					data->frame_vars.size(),
					base::strConcat(
						"Frame var count mismatch at frame ",
						frame_index,
						" (",
						data->function_name,
						"): expected ",
						expected_vars->size(),
						", got ",
						data->frame_vars.size()
					)
				);

				for (usize var_index = 0; var_index < expected_vars->size(); var_index++) {
					const auto& [expected_var_name, expected_var_type]
						= expected_vars->at(var_index);
					const auto& actual_var = data->frame_vars.at(var_index);

					assertTrue(
						actual_var.name.has_value(),
						base::strConcat(
							"Frame var ", var_index, " at frame ", frame_index, " has no name"
						)
					);
					assertTrue(
						actual_var.type.has_value(),
						base::strConcat(
							"Frame var ", var_index, " at frame ", frame_index, " has no type"
						)
					);

					assertEqual(
						expected_var_name,
						*actual_var.name,
						base::strConcat(
							"Frame var name mismatch at frame ", frame_index, " index ", var_index
						)
					);
					assertEqual(
						expected_var_type,
						*actual_var.type,
						base::strConcat(
							"Frame var type mismatch at frame ", frame_index, " index ", var_index
						)
					);
				}
			}
			return *this;
		}

		FlowSimulator& enforceFrameVarValue(u64 frame_index, base::StrID name, u64 expected) {
			auto data = vm::api::debuggerGetStackFrameData(pid, thread_id, frame_index);
			if (!data) assertTrue(false, vm::api::errorToString(data.error()));

			for (const auto& var: data->frame_vars) {
				if (!var.name.has_value() || *var.name != name) continue;

				auto opt_data = var.value->readData();
				assertTrue(
					opt_data.has_value(), base::strConcat("Failed to read the value of '", name, "'")
				);
				auto* primitive
					= std::get_if<vm::interpreted_data_variant::Primitive>(&opt_data.value());
				assertTrue(
					primitive != nullptr, base::strConcat("'", name, "' is not a primitive value")
				);
				assertEqual(
					expected, primitive->value, base::strConcat("Value of '", name, "' mismatch")
				);
				return *this;
			}

			assertTrue(
				false, base::strConcat("Frame ", frame_index, " has no variable named '", name, "'")
			);
			return *this;
		}

		/// Starts evaluating @p file and keeps its pending result (LIFO) for `awaitExprResult`.
		FlowSimulator& loadRuntimeExpr(const fs::File& file) {
			// Capture the caller position while still paused; `ret_from_expr` returns here.
			auto position = vm::api::getCurrentPosition(pid);
			if (!position) assertTrue(false, vm::api::errorToString(position.error()));

			auto response = vm::api::executeRuntimeExpr(pid, thread_id, file);
			if (!response) assertTrue(false, vm::api::errorToString(response.error()));

			pending_expr_results.push_back(PendingExpr{
				.future          = std::move(response.value()),
				.caller_function = position->function_name,
				.caller_instr    = position->instr_number,
			});
			return *this;
		}

		FlowSimulator& awaitExprResult(
			const std::vector<u64>& expected_result, u64 timeout = DEFAULT_EXPR_TIMEOUT
		) {
			std::vector<ExpectedValue> expected;
			expected.reserve(expected_result.size());
			for (auto value: expected_result) expected.emplace_back(value);
			return awaitExprResultValues(expected, timeout);
		}

		FlowSimulator& awaitExprResultValues(
			const std::vector<ExpectedValue>& expected_result,
			u64                               timeout = DEFAULT_EXPR_TIMEOUT
		) {
			auto pending = popPendingExpr();
			assertTrue(
				pending.future.wait_for(std::chrono::milliseconds(timeout))
					== std::future_status::ready,
				"Expression result did not arrive within the timeout"
			);
			assertExitValues(api::ExitValue{ pending.future.get() }, expected_result);
			awaitBreakpoint(pending.caller_function, pending.caller_instr);
			return *this;
		}

		FlowSimulator& evalExprExpectBreakpoint(const fs::File& file) {
			loadRuntimeExpr(file);
			awaitBreakpoint();
			assertTrue(
				pending_expr_results.back().future.wait_for(std::chrono::seconds(0))
					!= std::future_status::ready,
				"Expected expression evaluation to pause on breakpoint, but it completed"
			);
			return *this;
		}

		FlowSimulator& evalExprExpectLoadError(
			const fs::File& file, std::string_view expected_error_piece = ""
		) {
			auto response = vm::api::executeRuntimeExpr(pid, thread_id, file);
			assertTrue(
				!response.has_value(),
				"Expected expression to be rejected, but it evaluated successfully"
			);

			const std::string message = vm::api::errorToString(response.error());
			assertTrue(
				std::holds_alternative<vm::api::LoadProgramError>(response.error()),
				base::strConcat("Expected LoadProgramError, got: ", message)
			);
			assertTrue(
				expected_error_piece.empty()
					|| message.find(expected_error_piece) != std::string::npos,
				base::strConcat(
					"Expression error does not mention '",
					expected_error_piece,
					"'. The error was: ",
					message
				)
			);
			return *this;
		}

		FlowSimulator& evalExprProvideInputAfter(
			const fs::File&         file,
			const std::string&      input,
			usize                   delay_ms,
			const std::vector<u64>& expected_result
		) {
			std::thread poster([this, &input, delay_ms] {
				std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
				provideInput(input);
			});

			loadRuntimeExpr(file);
			poster.join();
			return awaitExprResult(expected_result);
		}

		/// Asserts the already-loaded expression is still running past the caller-side budget. The VM
		/// no longer owns a timeout, so the caller decides when to give up; the future stays pending
		/// and is later consumed by `awaitExprResult`.
		FlowSimulator& evalExprExpectTimeout(u64 timeout_ms = DEFAULT_EXPR_TIMEOUT) {
			assertTrue(
				pending_expr_results.back().future.wait_for(std::chrono::milliseconds(timeout_ms))
					!= std::future_status::ready,
				"Expected the expression to still be running, but it completed"
			);

			return *this;
		}

		FlowSimulator& provideInput(const std::string& input) {
			auto posted = vm::api::input(pid, input);
			if (!posted)
				assertTrue(
					false,
					base::strConcat("Posting input failed: ", vm::api::errorToString(posted.error()))
				);
			return *this;
		}

		/// Evaluates an expression that is expected to panic while it runs, and asserts the panic
		/// message contains @p expected_piece. Unlike a load-time rejection, a runtime panic leaves
		/// the process in the `Panicked` state, which is where the message is read from.
		FlowSimulator& evalExprExpectPanic(const fs::File& file, std::string_view expected_piece) {
			auto response = vm::api::executeRuntimeExpr(pid, thread_id, file);
			if (!response) assertTrue(false, vm::api::errorToString(response.error()));

			// The expression panics before `ret_from_expr`, so its future never completes; wait for
			// the process to report the panic instead.
			base::Optional<vm::api::ExecutionPanicked> panicked_status;
			for (usize attempt = 0; attempt < 200 && !panicked_status.has_value(); attempt++) {
				auto status = vm::api::getExecutionStatus(pid);
				if (!status)
					assertTrue(
						false,
						base::strConcat(
							"Failed to read the execution status: ",
							vm::api::errorToString(status.error())
						)
					);
				if (auto* panicked = std::get_if<vm::api::ExecutionPanicked>(&status.value()))
					panicked_status = *panicked;
				else
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			assertTrue(panicked_status.has_value(), "Expected the process to have panicked");
			assertTrue(
				panicked_status->error_message.find(expected_piece) != std::string::npos,
				base::strConcat(
					"Panic message '",
					panicked_status->error_message,
					"' does not mention '",
					expected_piece,
					"'"
				)
			);
			return *this;
		}

		FlowSimulator& pause() {
			auto pause_res = vm::api::pause(pid, thread_id);
			if (!pause_res)
				assertTrue(
					false,
					base::strConcat("Pause failed: ", vm::api::errorToString(pause_res.error()))
				);
			return *this;
		}

		/// Stops the thread. After this call the process is not usable for evaluation anymore,
		/// and the test should call `cleanup()` immediately.
		FlowSimulator& stop() {
			auto stop_res = vm::api::stop(pid);
			if (!stop_res)
				assertTrue(
					false, base::strConcat("Stop failed: ", vm::api::errorToString(stop_res.error()))
				);
			return *this;
		}

		FlowSimulator& resume() {
			auto resume_res = vm::api::resume(pid, thread_id);
			if (!resume_res)
				assertTrue(
					false,
					base::strConcat("Resume failed: ", vm::api::errorToString(resume_res.error()))
				);
			return *this;
		}

		FlowSimulator& step(u64 count = 1) {
			for (u64 i = 0; i < count; i++) {
				auto step_res = vm::api::step(pid, thread_id);
				if (!step_res)
					assertTrue(
						false,
						base::strConcat("Step failed: ", vm::api::errorToString(step_res.error()))
					);
			}
			return *this;
		}

		FlowSimulator& finishAndAssertExitValue(i64 expected_exit_val) {
			auto resume_res = vm::api::resume(pid, thread_id);
			if (!resume_res)
				assertTrue(
					false,
					base::strConcat("Resume failed: ", vm::api::errorToString(resume_res.error()))
				);

			auto join_res = vm::api::join(pid, thread_id);
			if (!join_res)
				assertTrue(
					false, base::strConcat("Join failed: ", vm::api::errorToString(join_res.error()))
				);

			auto exit_val_res = vm::api::getExitValue(pid);
			if (!exit_val_res)
				assertTrue(
					false,
					base::strConcat(
						"Failed to get exit value: ", vm::api::errorToString(exit_val_res.error())
					)
				);
			assertExitValue(exit_val_res.value(), { u64(expected_exit_val) });
			return *this;
		}

		/// Kills the process spawned by this simulator. Must be called after every test's
		/// command chain, otherwise the Supervisor destructor warns about leftover processes.
		FlowSimulator& cleanup() {
			auto kill_res = vm::api::kill(pid);
			if (!kill_res)
				assertTrue(
					false, base::strConcat("Kill failed: ", vm::api::errorToString(kill_res.error()))
				);
			return *this;
		}

	private:
		void assertExitValue(
			const vm::api::ExitValue& exit_value, const std::vector<u64>& expected_result
		) const {
			std::vector<ExpectedValue> expected;
			expected.reserve(expected_result.size());
			for (auto value: expected_result) expected.emplace_back(value);
			assertExitValues(exit_value, expected);
		}

		void assertExitValues(
			const vm::api::ExitValue& exit_value, const std::vector<ExpectedValue>& expected_result
		) const {
			assertTrue(
				std::holds_alternative<std::vector<Ref<vm::IVMValue>>>(exit_value),
				"Expected vector of IVMValue from expression"
			);
			auto& ret_vals = std::get<std::vector<Ref<vm::IVMValue>>>(exit_value);
			assertEqual(expected_result.size(), ret_vals.size(), "Return value count mismatch");

			for (usize i = 0; i < expected_result.size(); ++i)
				assertValue(*ret_vals[i], expected_result[i]);
		}

		template<class Actual>
		void assertValue(const Actual& actual, const ExpectedValue& expected) const {
			std::visit(
				[&](const auto& exp) {
					using Exp = std::decay_t<decltype(exp)>;

					if constexpr (std::is_same_v<Exp, Primitive>) {
						auto data
							= actual.template readData<vm::interpreted_data_variant::Primitive>();
						assertTrue(data.has_value(), "Expected a primitive value");
						assertEqual(exp.value, data->value, "Primitive value mismatch");
					} else if constexpr (std::is_same_v<Exp, Ptr>) {
						auto data
							= actual.template readData<vm::interpreted_data_variant::Pointer>();
						assertTrue(data.has_value(), "Expected a pointer value");
						assertTrue(data->referenced.has_value(), "Expected a non-null pointer");
						if (exp.pointee) assertValue(**data->referenced, *exp.pointee);
					} else if constexpr (std::is_same_v<Exp, Struct>) {
						auto data = actual.template readData<vm::interpreted_data_variant::Data>();
						assertTrue(data.has_value(), "Expected a struct value");
						assertEqual(
							exp.fields.size(), data->fields.size(), "Struct field count mismatch"
						);
						for (const auto& [name, field_expected]: exp.fields) {
							assertTrue(
								data->field_name_map.contains(name),
								base::strConcat("Missing struct field '", name, "'")
							);
							usize index = data->field_name_map.at(name);
							assertValue(*data->fields.at(index).value, field_expected);
						}
					} else if constexpr (std::is_same_v<Exp, Variant>) {
						auto data
							= actual.template readData<vm::interpreted_data_variant::Variant>();
						assertTrue(data.has_value(), "Expected a variant value");
						assertEqual(exp.type_tag, data->type_tag, "Variant tag mismatch");
						if (exp.inner) assertValue(*data->referenced, *exp.inner);
					} else if constexpr (std::is_same_v<Exp, Table>) {
						auto data = actual.template readData<vm::interpreted_data_variant::Table>();
						assertTrue(data.has_value(), "Expected a table value");
						assertEqual(exp.elements.size(), usize(data->size), "Table size mismatch");
						for (usize i = 0; i < exp.elements.size(); i++)
							assertValue(*data->get(i), exp.elements[i]);
					}
				},
				expected.get()
			);
		}

		void assertTrue(bool condition, std::string_view err) const {
			assert_true_fn(condition, err);
		}

		template<typename T, typename U>
		void assertEqual(const T& expected, const U& actual, std::string_view err) const {
			assertTrue(
				expected == actual,
				base::strConcat(
					err,
					": expected ",
					base::escapeString(base::strConcat(expected)),
					", got ",
					base::escapeString(base::strConcat(actual))
				)
			);
		}

		std::function<void(bool, std::string_view)> assert_true_fn;
		vm::PID                                     pid;
		vm::api::ThreadID                           thread_id;
	};
}
