#pragma once

#include <events/emitter.hpp>

#include <base/collections/optional.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>

#include <tester/tester.hpp>

#include <vm/api/data/api_error.hpp>
#include <vm/api/vm.hpp>
#include <vm/core/safe/vmvalue/safe_vmvalue.hpp>

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string_view>
#include <vector>

namespace vm::test {
	class FlowSimulator {
		struct LateEvaluation;

		std::deque<LateEvaluation> late_evals;
		using ResT = std::vector<Ref<SafeVMValue>>;

		std::deque<std::pair<ResT, u64>> late_result;

		std::condition_variable cv;
		std::mutex              mt;

		struct LateEvaluation {
			events::Listener<ResT> listener;

			LateEvaluation(FlowSimulator& simulator, Ref<events::Emitter<ResT>> emitter):
				  listener([&simulator, self = this](ResT res) {
					  CORE_ASSERT(
						  simulator.late_evals.size(), "there must be some evaluation not completed"
					  );
					  CORE_ASSERT(
						  &simulator.late_evals.back() == self,
						  "I am at the highest evaluation to be performed"
					  );

					  size_t my_idx = simulator.late_evals.size();
					  simulator.late_result.emplace_back(res, my_idx);
					  simulator.cv.notify_all();
					  simulator.late_evals.pop_back();
				  }) {
				emitter->attachListener(this->listener);
			}
		};

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
			assertTrue(run_res.has_value(), "Run failed");
			return *this;
		}

		FlowSimulator& awaitBreakpoint(base::StrID expected_func, u64 expected_instr) {
			auto bp_res = vm::api::waitForBreakpoint(pid);
			assertTrue(bp_res.has_value(), "Wait for breakpoint failed");
			assertEqual(expected_func, bp_res->function_name, "Breakpoint function mismatch");
			assertEqual(expected_instr, bp_res->instr_number, "Breakpoint instruction mismatch");
			return *this;
		}

		FlowSimulator& evalExprNormal(const fs::File& file, const std::vector<u64>& expected_result) {
			auto response = vm::api::executeRuntimeExprFromFile(pid, thread_id, file);
			if (!response) assertTrue(false, vm::api::errorToString(response.error()));

			assertExitValue(response.value(), expected_result);
			return *this;
		}

		FlowSimulator& evalExprExpectBreakpoint(
			const fs::File& file, base::StrID expected_func, u64 expected_instr
		) {
			auto response = vm::api::executeRuntimeExprFromFile(pid, thread_id, file);
			assertTrue(
				!response.has_value(),
				"Expected expression evaluation to pause on breakpoint, but it completed"
			);

			auto [ref, reason] = v_get(response.error(), vm::api::IncompleteExprEval);

			CORE_ASSERT(ref, "we should get a valid ref to the eventual emitter of the response");

			late_evals.emplace_back(*this, *ref.toOpt());

			return awaitBreakpoint(expected_func, expected_instr);
		}

		FlowSimulator& awaitExprCompletion(const std::vector<u64>& expected) {
			std::unique_lock lock(mt);
			cv.wait(lock, [&] { return bool(late_result.size()); });
			auto res = late_result.front().first;
			late_result.pop_front();

			using ApiResT          = std::vector<Ref<IVMValue>>;
			api::ExitValue api_res = ApiResT{};
			for (auto safe_ref: res) v_get(api_res, ApiResT).emplace_back(safe_ref.get());

			assertExitValue(api_res, expected);
			return *this;
		}

		FlowSimulator& resume() {
			auto resume_res = vm::api::resume(pid, thread_id);
			assertTrue(resume_res.has_value(), "Resume failed");
			return *this;
		}

		FlowSimulator& finishAndAssertExitValue(i64 expected_exit_val) {
			auto resume_res = vm::api::resume(pid, thread_id);
			assertTrue(resume_res.has_value(), "Resume failed");

			auto join_res = vm::api::join(pid, thread_id);
			assertTrue(join_res.has_value(), "Join failed");

			auto exit_val_res = vm::api::getExitValue(pid);
			assertTrue(exit_val_res.has_value(), "Failed to get exit value");
			assertExitValue(exit_val_res.value(), { u64(expected_exit_val) });
			return *this;
		}

		/// Kills the process spawned by this simulator. Must be called after every test's
		/// command chain, otherwise the Supervisor destructor warns about leftover processes.
		FlowSimulator& cleanup() {
			auto kill_res = vm::api::kill(pid);
			assertTrue(kill_res.has_value(), "Kill failed");
			return *this;
		}

	private:
		void assertExitValue(
			const vm::api::ExitValue& exit_value, const std::vector<u64>& expected_result
		) const {
			assertTrue(
				std::holds_alternative<std::vector<Ref<vm::IVMValue>>>(exit_value),
				"Expected vector of IVMValue from expression"
			);
			auto& ret_vals = std::get<std::vector<Ref<vm::IVMValue>>>(exit_value);
			assertEqual(expected_result.size(), ret_vals.size(), "Return value count mismatch");

			for (usize i = 0; i < expected_result.size(); ++i) {
				auto opt_data = ret_vals[i]->readData();
				assertTrue(opt_data.has_value(), "Failed to read data from return value");
				auto* primitive
					= std::get_if<vm::interpreted_data_variant::Primitive>(&opt_data.value());
				assertTrue(primitive != nullptr, "Expected Primitive return value");
				assertEqual(
					expected_result[i],
					primitive->value,
					"Return value mismatch at index " + std::to_string(i)
				);
			}
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
