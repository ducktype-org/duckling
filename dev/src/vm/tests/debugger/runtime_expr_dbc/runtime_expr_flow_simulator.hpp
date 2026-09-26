#pragma once

#include <events/emitter.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/data/response.hpp>
#include <vm/api/data/status.hpp>
#include <vm/api/vm.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
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
		using ResT = std::vector<Ref<SafeVMValue>>;

		struct LateEvaluation {
			events::Listener<ResT> listener;

			LateEvaluation(FlowSimulator& simulator, Ref<events::Emitter<ResT>> emitter):
				  listener([&simulator, self = this](ResT res) {
					  {
						  std::lock_guard lock(simulator.mt);
						  simulator.late_result.emplace_back(std::move(res), self);
					  }
					  simulator.cv.notify_all();
				  }) {
				emitter->attachListener(this->listener);
			}
		};

		std::deque<LateEvaluation> late_evals;

		std::deque<LateEvaluation*> pending_late_evals;

		std::deque<std::pair<ResT, LateEvaluation*>> late_result;

		std::condition_variable cv;
		std::mutex              mt;

		void registerLateEvaluation(Ref<events::Emitter<ResT>> emitter) {
			late_evals.emplace_back(*this, emitter);
			pending_late_evals.push_back(&late_evals.back());
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

		FlowSimulator& awaitBreakpoint(base::StrID expected_func, u64 expected_instr) {
			auto bp_res = vm::api::waitForBreakpoint(pid);
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

		FlowSimulator& evalExprNormal(const fs::File& file, const std::vector<u64>& expected_result) {
			auto response = vm::api::executeRuntimeExpr(pid, thread_id, file);
			if (!response) assertTrue(false, vm::api::errorToString(response.error()));

			assertExitValue(response.value(), expected_result);
			return *this;
		}

		FlowSimulator& evalExprExpectValues(
			const fs::File& file, const std::vector<ExpectedValue>& expected_result
		) {
			auto response = vm::api::executeRuntimeExpr(pid, thread_id, file);
			if (!response) assertTrue(false, vm::api::errorToString(response.error()));

			assertExitValues(response.value(), expected_result);
			return *this;
		}

		FlowSimulator& evalExprExpectBreakpoint(const fs::File& file) {
			auto response = vm::api::executeRuntimeExpr(pid, thread_id, file);
			assertTrue(
				!response.has_value(),
				"Expected expression evaluation to pause on breakpoint, but it completed"
			);

			registerIncompleteEval(response.error());
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

			auto response = vm::api::executeRuntimeExpr(pid, thread_id, file);
			poster.join();

			if (!response)
				assertTrue(
					false,
					base::strConcat(
						"Expected the expression to complete after the input was posted, but it "
						"failed: ",
						vm::api::errorToString(response.error())
					)
				);
			assertExitValue(response.value(), expected_result);
			return *this;
		}

		FlowSimulator& evalExprExpectTimeout(const fs::File& file) {
			const auto start    = std::chrono::steady_clock::now();
			auto       response = vm::api::executeRuntimeExpr(pid, thread_id, file);
			const auto elapsed  = std::chrono::steady_clock::now() - start;

			assertTrue(
				!response.has_value(), "Expected the expression to time out, but it completed"
			);

			const auto reason = registerIncompleteEval(response.error());
			assertTrue(
				reason.find("timeout") != std::string::npos,
				base::strConcat("Expected a timeout error, got: ", reason)
			);
			assertTrue(
				elapsed >= std::chrono::milliseconds(450),
				"Evaluation terminated before the 0,5 s budget elapsed"
			);
			assertTrue(
				elapsed <= std::chrono::milliseconds(700),
				"Evaluation took much longer than the 0,5 s budget"
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
			assertTrue(!response.has_value(), "Expected the expression to panic, but it completed");

			auto status = vm::api::getExecutionStatus(pid);
			if (!status)
				assertTrue(
					false,
					base::strConcat(
						"Failed to read the execution status: ",
						vm::api::errorToString(status.error())
					)
				);

			auto* panicked = std::get_if<vm::api::ExecutionPanicked>(&status.value());
			assertTrue(panicked != nullptr, "Expected the process to have panicked");
			assertTrue(
				panicked->error_message.find(expected_piece) != std::string::npos,
				base::strConcat(
					"Panic message '",
					panicked->error_message,
					"' does not mention '",
					expected_piece,
					"'"
				)
			);
			return *this;
		}

		/// Pauses the thread (blocking until it is actually `Paused`) and asserts the pause
		/// happened at the expected position.
		FlowSimulator& pause(base::StrID expected_func, u64 expected_instr) {
			auto pause_res = vm::api::pause(pid, thread_id);
			if (!pause_res)
				assertTrue(
					false,
					base::strConcat("Pause failed: ", vm::api::errorToString(pause_res.error()))
				);
			assertEqual(expected_func, pause_res->function_name, "Pause function mismatch");
			assertEqual(expected_instr, pause_res->instr_number, "Pause instruction mismatch");
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

		FlowSimulator& awaitExprCompletion(const std::vector<u64>& expected) {
			using ApiResT = std::vector<Ref<IVMValue>>;

			ResT res;
			{
				std::unique_lock lock(mt);
				cv.wait(lock, [&] { return bool(late_result.size()); });

				auto& [result, evaluation] = late_result.front();
				res                        = std::move(result);

				CORE_ASSERT(
					!pending_late_evals.empty() && pending_late_evals.back() == evaluation,
					"Evaluations must complete in LIFO order"
				);
				late_result.pop_front();
				pending_late_evals.pop_back();
			}

			api::ExitValue api_res = ApiResT{};
			for (auto safe_ref: res) v_get(api_res, ApiResT).emplace_back(safe_ref.get());

			assertExitValue(api_res, expected);
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

		FlowSimulator& sleep(u64 milliseconds) {
			std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
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
		/// Registers the pending emitter of an incomplete evaluation for a later
		/// `awaitExprCompletion` and returns the reported reason.
		std::string registerIncompleteEval(const vm::api::ApiError& error) {
			auto [ref, reason] = v_get(error, vm::api::IncompleteExprEval);
			CORE_ASSERT(ref, "we should get a valid ref to the eventual emitter of the response");
			registerLateEvaluation(ref.get());
			return reason;
		}

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
