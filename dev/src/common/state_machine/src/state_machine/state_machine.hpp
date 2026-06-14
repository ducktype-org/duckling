/**
 * @file state_machine.hpp
 *
 * @brief State Machine is a small library that implements a generic,
 * type-safe finite state machine on top of `std::variant`.
 *
 * The library is built around two cooperating types:
 *  * `StateMachineDefinition` - a static description of the allowed
 *    transitions for a given pair of `States`/`Events` variants.
 *  * `StateMachine` - a runtime instance that owns the current state and
 *    dispatches incoming events.
 *  * `AtomicStateMachine` - a thread-safe wrapper around `StateMachine`
 *  * `WaitableStateMachine` - a thread-safe wrapper around `StateMachine` with additional ability
 * to wait until a state machine reaches a specified state.
 *
 * Functionalities
 * ===============
 *
 * Defining the alphabet
 * ---------------------
 *
 * A state machine is parameterized by three template arguments:
 *  * `States` - an `std::variant` whose alternatives are the states the
 *    machine can be in. Each alternative may carry its own data.
 *  * `Events` - an `std::variant` whose alternatives are the events that can be dispatched to the
 * 	   machine.
 *  * `ErrorT` - error type used by actions. Defaults to `std::string`.
 *
 * Configuring transitions
 * -----------------------
 *
 * Three main overloads are provided on `StateMachineDefinition` for registering
 * transitions (see `state_machine_example.cpp`):
 *  * `addTransition<State, Event>` - registers a transition for a single
 *    `(State, Event)` pair.
 *  * `addTransitions<Event, FromStates...>` - registers the same transition
 *    for several source states at once.
 *  * `addTransitionFromAllStates<Event>` - registers a transition for the
 *    given event from every state in the `States` variant. Useful for
 *    events that are always valid (e.g. `Kill` in case of DVM).
 *
 * Actions
 * ------------------
 *
 * Each transition is composed of a callable:
 *  * `action` - executed when the transition is performed. Returns `std::expected<States, ErrorT>`
 * 	  - the new state on success or an error on failure.
 *
 * Handling events
 * ---------------
 *
 * `StateMachine::handleEvent(event)` returns a
 * `base::Optional<std::expected<void, ErrorT>>`:
 *  * `std::nullopt` - when the machine is in a state for which the action is not specified as a
 * 	   valid transition. The machine is unchanged.
 *  * `std::expected<void, ErrorT>{}` - the transition fired successfully and the state was updated.
 * 	   For standard actions, the state has been updated to the value the action returned.
 *  * `std::unexpected(error)` - the action reported an error. The machine stays in its current
 * state.
 *
 * Retrieving the current state
 * ----------------------------
 *
 * The current state can be inspected via `StateMachine::getState()`, which
 * returns `const States&` variant.
 */

#pragma once

#include <base/collections/optional.hpp>
#include <base/comptime/type_traits.hpp>
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/pointers/ref.hpp>

#include <array>
#include <condition_variable>
#include <expected>
#include <functional>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace state_machine {
#define ASSERT_TRANSITION_NOT_REGISTERED(STATE_IDX, EVENT_IDX) \
	CORE_ASSERT(                                               \
		!transitions[STATE_IDX][EVENT_IDX].has_value(),        \
		"Transition for this State (variant index: ",          \
		STATE_IDX,                                             \
		") and Event (variant index: ",                        \
		EVENT_IDX,                                             \
		") is already defined!"                                \
	);

	// FD for friend.
	template<typename States, typename Events, typename ErrorT>
	class StateMachine;

	/**
	 * @brief Static description of a state machine - the set of allowed
	 * `(state, event)` transitions and their handlers.
	 *
	 * A `StateMachineDefinition` is meant to be configured once and then
	 * passed to one or more `StateMachine` instances which carry the
	 * runtime state. Transitions are stored in a fixed-size
	 * `(NUM_STATES x NUM_EVENTS)` table keyed by the variant indices.
	 *
	 * @tparam States Variant of state alternatives.
	 * @tparam Events Variant of event alternatives.
	 * @tparam ErrorT Error type used by actions.
	 */
	template<base::IsVariant States, base::IsVariant Events, typename ErrorT = std::string>
	class StateMachineDefinition final {
	public:
		friend class StateMachine<States, Events, ErrorT>;
		using ActionResultT = std::expected<States, ErrorT>;

	private:
		// Type-erased handlers, so they can all be stored in the same array.
		using RawAction = std::function<ActionResultT(const States&, const Events&)>;

		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		using StronglyTypedAction = std::function<ActionResultT(const State&, const Event&)>;

		template<typename Event>
		using GenericAction = std::function<ActionResultT(const States&, const Event&)>;

		/**
		 * @brief Represents a single transition entry for a (State, Event) pair.
		 */
		struct TransitionEntry {
			RawAction action;  ///< Action that returns the new state.
		};

		static constexpr usize NUM_STATES = std::variant_size_v<States>;
		static constexpr usize NUM_EVENTS = std::variant_size_v<Events>;
		/**
		 * @brief An allowed transitions map. For each (State, Event) pair stores the optional
		 * `Action` to be performed on the transition. Empty optional means that this
		 * `Event` in the `State` is not allowed.
		 */
		std::array<std::array<base::Optional<TransitionEntry>, NUM_EVENTS>, NUM_STATES> transitions;

	public:
		/**
		 * @brief Registers a transition for a single `(State, Event)` pair.
		 *
		 * @tparam State Source state type. Must be an alternative of `States`.
		 * @tparam Event Triggering event type. Must be an alternative of `Events`.
		 *
		 * @param action Callable invoked when the transition fires. Returns
		 *               the new state on success, or
		 *               `std::unexpected(error)` on failure. The new state
		 *               is allowed to be a different alternative of `States`.
		 */
		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		void addTransition(StronglyTypedAction<State, Event> action) {
			const usize state_idx = base::variantTypeIndex<States, State>();
			const usize event_idx = base::variantTypeIndex<Events, Event>();
			ASSERT_TRANSITION_NOT_REGISTERED(state_idx, event_idx);

			RawAction raw_action
				= [action = std::move(action)](const States& s, const Events& e) -> ActionResultT {
				return action(std::get<State>(s), std::get<Event>(e));
			};

			transitions.at(state_idx).at(event_idx) = TransitionEntry{ std::move(raw_action) };
		}

		/**
		 * @brief Registers the same transition for several source states.
		 *
		 * Useful when the same `Action` should fire from any of several states.
		 *
		 * @tparam Event      Triggering event type. Must be an alternative
		 *                    of `Events`.
		 * @tparam FromStates Source states from which the event is
		 *                    accepted. Each must be an alternative of `States`.
		 */
		template<typename Event, typename... FromStates>
		requires(base::IsVariantMember<FromStates, States> && ...)
		     && base::IsVariantMember<Event, Events>
		void addTransitions(const GenericAction<Event>& action) {
			(addTransitionInternal<FromStates, Event>(action), ...);
		}

		/**
		 * @brief Registers a transition for `Event` from every state.
		 *
		 * Convenience wrapper around `addTransitions` covering all
		 * alternatives of `States`. Useful for events that are always
		 * valid (e.g. `Kill` in case of DVM).
		 *
		 * @tparam Event Triggering event type. Must be an alternative of `Events`.
		 */
		template<typename Event>
		requires base::IsVariantMember<Event, Events>
		void addTransitionFromAllStates(const GenericAction<Event>& action) {
			[this, &action]<usize... Is>(std::index_sequence<Is...>) {
				(this->addTransitionInternal<std::variant_alternative_t<Is, States>, Event>(action),
				 ...);
			}(std::make_index_sequence<NUM_STATES>{});
		}

		/**
		 * @brief Returns the entry registered for the given
		 * `(State, Event)` pair, if it exists.
		 *
		 * @return `base::Optional` holding a `CRef` to the entry,
		 *         or empty if no transition was registered.
		 */
		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		[[nodiscard]] base::Optional<CRef<TransitionEntry>> getTransition() const {
			const usize state_idx = base::variantTypeIndex<States, State>();
			const usize event_idx = base::variantTypeIndex<Events, Event>();

			CORE_ASSERT(state_idx < NUM_STATES && event_idx < NUM_EVENTS, "Index out of bounds");
			const auto& entry = transitions.at(state_idx).at(event_idx);
			if (entry.has_value()) return CRef(&entry.value());
			return {};
		}

	private:
		/**
		 * @brief Shared logic used by `addTransitions` and
		 * `addTransitionFromAllStates`.
		 *
		 * Stores the user-supplied generic handlers in the type-erased
		 * transition table.
		 */
		template<typename State, typename Event>
		void addTransitionInternal(const GenericAction<Event>& action) {
			const usize state_idx = base::variantTypeIndex<States, State>();
			const usize event_idx = base::variantTypeIndex<Events, Event>();
			ASSERT_TRANSITION_NOT_REGISTERED(state_idx, event_idx);

			// Wrapper over action so it can be stored next to strongly-typed actions.
			RawAction raw_action = [action](const States& s, const Events& e) {
				return action(s, std::get<Event>(e));
			};

			transitions.at(state_idx).at(event_idx) = TransitionEntry{ std::move(raw_action) };
		}
	};

#undef ASSERT_TRANSITION_NOT_REGISTERED

	/**
	 * @brief Runtime instance of a state machine.
	 *
	 * Combines a current state with a reference to a configured
	 * `StateMachineDefinition`. Events are dispatched through
	 * `handleEvent`.
	 *
	 * @tparam States Variant of state alternatives. Must match the definition's `States`.
	 * @tparam Events Variant of event alternatives. Must match the definition's `Events`.
	 * @tparam ErrorT Error type used by actions. Must match the definition's `ErrorT`.
	 */
	template<typename States, typename Events, typename ErrorT = std::string>
	class StateMachine final {
	public:
		using ResultT         = std::expected<void, ErrorT>;
		using HandleResult    = base::Optional<ResultT>;
		using StateMachineDef = StateMachineDefinition<States, Events, ErrorT>;
		using RawAction       = typename StateMachineDef::RawAction;

		/**
		 * @brief Constructs a state machine in the given initial state.
		 *
		 * @param initial_state State the machine starts in.
		 * @param def           Definition describing the allowed transitions.
		 */
		StateMachine(States initial_state, CRef<StateMachineDef> def):
			  current_state(std::move(initial_state)),
			  definition(def) {}

		/**
		 * @brief Dispatches an event to the machine.
		 *
		 * Looks up the entry for `(current_state, event)`, then:
		 *  1. evaluates the action; on error returns
		 *     `std::unexpected(error)` and does not change state,
		 *  2. on success moves the new state into the machine and returns
		 *     `std::expected<void, ErrorT>{}`.
		 *
		 * @return * `std::nullopt` if no transition is registered for the
		 *           current `(state, event)` pair.
		 *         * `std::expected<void, ErrorT>{}` if the transition
		 *           fired successfully.
		 *         * `std::unexpected(error)` if the action reported an error. The state is not
		 * changed.
		 */
		HandleResult handleEvent(const Events& event) {
			auto maybe_transition = std::visit(
				[&](auto&& inner_state, auto&& inner_event) {
					using State = std::decay_t<decltype(inner_state)>;
					using Event = std::decay_t<decltype(inner_event)>;
					return definition->template getTransition<State, Event>();
				},
				current_state,
				event
			);

			if (!maybe_transition.has_value()) return std::nullopt;
			const auto& transition = maybe_transition.value();
			const auto& raw_action = transition->action;

			auto action_result = raw_action(current_state, event);
			if (!action_result.has_value())
				return HandleResult{ ResultT{ std::unexpected(std::move(action_result.error())) } };

			current_state = std::move(action_result.value());
			return HandleResult{ ResultT{} };
		}

		/**
		 * @brief Returns a CRef to the current state.
		 */
		[[nodiscard]] const States& getState() const { return current_state; }

	private:
		States current_state;              ///< Current state of the machine.
		CRef<StateMachineDef> definition;  ///< The definition the state machine was configured with.
	};

	/**
	 * @brief Thread-safe wrapper around `StateMachine`.
	 *
	 * Handles access to an underlying `StateMachine` with a `std::mutex`.
	 *
	 * @tparam States Variant of state alternatives. Must match the
	 *                definitions `States`.
	 * @tparam Events Variant of event alternatives. Must match the
	 *                definitions `Events`.
	 * @tparam ErrorT Error type used by actions. Must match
	 *                the definitions `ErrorT`.
	 */
	template<typename States, typename Events, typename ErrorT = std::string>
	class AtomicStateMachine final {
	public:
		using InnerMachine    = StateMachine<States, Events, ErrorT>;
		using ResultT         = typename InnerMachine::ResultT;
		using HandleResult    = typename InnerMachine::HandleResult;
		using StateMachineDef = typename InnerMachine::StateMachineDef;

		AtomicStateMachine(States initial_state, CRef<StateMachineDef> def):
			  machine(std::move(initial_state), def) {}

		AtomicStateMachine(const AtomicStateMachine&)            = delete;
		AtomicStateMachine& operator=(const AtomicStateMachine&) = delete;
		AtomicStateMachine(AtomicStateMachine&&)                 = delete;
		AtomicStateMachine& operator=(AtomicStateMachine&&)      = delete;

		/**
		 * @brief Dispatches an event to the machine under a lock.
		 * See `StateMachine::handleEvent` for more info.
		 */
		HandleResult handleEvent(const Events& event) {
			std::lock_guard lock(mutex);
			return machine.handleEvent(event);
		}

		/**
		 * @brief Runs a callback over the current state under a lock.
		 * @note The reference passed to `f` is only valid for the duration of the call.
		 * @note `f` must not invoke `handleEvent` on this machine.
		 *
		 * @tparam F  Callable invocable with `const States&`.
		 * @param  f  Callback invoked with the current state.
		 * @return    Whatever `f` returns.
		 */
		template<typename F>
		requires std::is_invocable_v<F, const States&>
		auto withState(F&& f) const -> decltype(auto) {
			std::lock_guard lock(mutex);
			return std::forward<F>(f)(machine.getState());
		}

		/**
		 * @brief Returns a copy of the current state.
		 * Available only when `States` is copy-constructible, otherwise use `withState`.
		 * @return The current machine state.
		 */
		States getStateCopy() const requires std::copy_constructible<States> {
			std::lock_guard lock(mutex);
			return machine.getState();
		}

	private:
		mutable std::mutex mutex;
		InnerMachine       machine;
	};

	/**
	 * @brief Thread-safe wrapper around `StateMachine` with a ability to block on the machine until
	 * it reaches a specific state and notifications on state changes.
	 *
	 * Handles access to an underlying `StateMachine` with a `std::mutex`.
	 *
	 * @tparam States Variant of state alternatives. Must match the
	 *                definitions `States`.
	 * @tparam Events Variant of event alternatives. Must match the
	 *                definitions `Events`.
	 * @tparam ErrorT Error type used by actions. Must match
	 *                the definitions `ErrorT`.
	 */
	template<typename States, typename Events, typename ErrorT = std::string>
	class WaitableStateMachine final {
		// We require copyable states since listeners need copies.
		static_assert(
			std::copy_constructible<States>, "WaitableStateMachine requires copyable states"
		);

	public:
		using InnerMachine    = StateMachine<States, Events, ErrorT>;
		using ResultT         = typename InnerMachine::ResultT;
		using HandleResult    = typename InnerMachine::HandleResult;
		using StateMachineDef = typename InnerMachine::StateMachineDef;

		/// Function representing a listener. Will be invoked by the state machine after a state change.
		using Listener = std::function<void(const States& from, const States& to, u64 generation)>;

		WaitableStateMachine(States initial_state, CRef<StateMachineDef> def):
			  machine(std::move(initial_state), def) {}

		WaitableStateMachine(const WaitableStateMachine&)            = delete;
		WaitableStateMachine& operator=(const WaitableStateMachine&) = delete;
		WaitableStateMachine(WaitableStateMachine&&)                 = delete;
		WaitableStateMachine& operator=(WaitableStateMachine&&)      = delete;

		/**
		 * @brief Add's a specific listener to this state machine. All listeners will be invoked on
		 * each state change performed by the machine in the order they were added.
		 */
		void subscribe(Listener listener) {
			std::lock_guard lk(listeners_mutex);
			listeners.push_back(std::move(listener));
		}

		/**
		 * @brief Dispatches an event to the machine under a lock, then when a successful transition
		 * was performed it invoked all listeners.
		 * See `StateMachine::handleEvent` for more info.
		 */
		HandleResult handleEvent(const Events& event) {
			States from_snapshot;
			States to_snapshot;
			u64    phase = 0;

			HandleResult res;
			{
				std::unique_lock lock(mutex);
				from_snapshot = machine.getState();
				res           = machine.handleEvent(event);

				if (!res.has_value()) return std::nullopt;
				if (!res->has_value()) return res;

				phase       = state_change_counter++;
				to_snapshot = machine.getState();
			}

			state_changed.notify_all();

			ordered.run(phase, [&] {
				std::vector<Listener> listeners_copy;
				{
					std::lock_guard lk(listeners_mutex);
					listeners_copy = listeners;
				}
				for (auto& listener: listeners_copy) listener(from_snapshot, to_snapshot, phase);
			});

			return HandleResult{ ResultT{} };
		}

		/**
		 * @brief Blocks until pred(state) is true.
		 * @return Returns a copy of the state.
		 */
		template<typename Pred>
		requires std::is_invocable_r_v<bool, Pred, const States&>
		States waitForState(Pred pred) const requires std::is_copy_constructible_v<States> {
			std::unique_lock lock(mutex);
			state_changed.wait(lock, [&]() { return pred(machine.getState()); });
			return machine.getState();
		}

		/**
		 * @brief Blocks until pred(state) is true and the state is newer than `since`.
		 *
		 * @detail: Use this, when state change changes may come often. And we may miss a state
		 * change when sleeping. For example when a thread goes from Paused -> Running -> Paused and
		 * we can't tell a difference between two Paused states.
		 *
		 * @return Returns a copy of the state.
		 */
		template<typename Pred>
		requires std::is_invocable_r_v<bool, Pred, const States&>
		States waitForFreshState(u64 since, Pred pred) const
			requires std::is_copy_constructible_v<States> {
			std::unique_lock lock(mutex);
			state_changed.wait(lock, [&]() {
				return state_change_counter > since && pred(machine.getState());
			});
			return machine.getState();
		}

		/**
		 * @brief Returns the count of state changes performed by this state machine.
		 * Usefull when using `waitForFreshState()`.
		 */
		u64 getStateChangeCount() const {
			std::lock_guard lock(mutex);
			return state_change_counter;
		}

		/**
		 * @brief Runs a callback over the current state under a lock.
		 * See `AtomicStateMachine::handleEvent` for more info.
		 */
		template<typename F>
		requires std::is_invocable_v<F, const States&>
		auto withState(F&& f) const -> decltype(auto) {
			std::lock_guard lock(mutex);
			return std::forward<F>(f)(machine.getState());
		}

		/**
		 * @brief Returns a copy of the current state.
		 * Available only when `States` is copy-constructible, otherwise use `withState`.
		 * @return The current machine state.
		 */
		States getStateCopy() const requires std::copy_constructible<States> {
			std::lock_guard lock(mutex);
			return machine.getState();
		}


	private:
		/**
		 * @brief Serializes listener calls in the state change order.
		 *
		 * @detail `handleEvent` changes the state under the state machine mutex and takes an
		 * increasing `phase` number, then releases the mutex before running listeners
		 * (a listener may block, so we must not hold the mutex). Without sync, two
		 * committed transitions could run their listeners out of order. `run(phase, fn)` blocks
		 * until `next_phase` reaches `phase`, runs `fn`, then advances `next_phase` via the RAII
		 * guard. The RAII guard is used so the counter gets incremented even if `fn` throws, and
		 * listeners observe transitions strictly in state change order.
		 */
		class OrderedListenerExecutor {
		public:
			template<typename Fn>
			void run(u64 phase, Fn&& fn) {
				std::unique_lock lock(m);
				cv.wait(lock, [&] { return next_phase == phase; });
				lock.unlock();
				PhaseGuard guard{ *this };

				std::forward<Fn>(fn)();
			}

		private:
			struct PhaseGuard {
				OrderedListenerExecutor& ex;

				~PhaseGuard() {
					{
						std::lock_guard lk(ex.m);
						++ex.next_phase;
					}
					ex.cv.notify_all();
				}
			};

			std::mutex              m;
			std::condition_variable cv;
			u64                     next_phase = 0;
		};

		InnerMachine machine;

		mutable std::mutex              mutex;          ///< Guards `handleEvent` and `getState*()`.
		mutable std::condition_variable state_changed;  ///< CV for `waitForState`
		u64                             state_change_counter{
            0
		};  ///< Number of times the state was changed in this state machine.

		OrderedListenerExecutor ordered;

		std::mutex listeners_mutex;
		std::vector<Listener>
			listeners;  ///< Functions to be invoked on each state change on the machine.
	};
}
