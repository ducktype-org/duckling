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
#include <base/types/ints.hpp>

#include <array>
#include <condition_variable>
#include <expected>
#include <functional>
#include <mutex>
#include <shared_mutex>
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
		// =========================================================
		// Normal transitions
		// =========================================================
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
		using ResultT         = base::Optional<std::expected<void, ErrorT>>;
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
		ResultT handleEvent(const Events& event) {
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

			// Standard action. Action returns the new state.
			auto action_result = raw_action(current_state, event);
			if (!action_result.has_value())
				return std::unexpected(std::move(action_result.error()));

			current_state = std::move(action_result.value());
			return std::expected<void, ErrorT>{};
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
	 *                definition's `States`.
	 * @tparam Events Variant of event alternatives. Must match the
	 *                definition's `Events`.
	 * @tparam ErrorT Error type used by guards and actions. Must match
	 *                the definition's `ErrorT`.
	 */
	template<typename States, typename Events, typename ErrorT = std::string>
	class AtomicStateMachine final {
	public:
		using InnerMachine    = StateMachine<States, Events, ErrorT>;
		using ResultT         = typename InnerMachine::ResultT;
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
		ResultT handleEvent(const Events& event) {
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
	 * @brief Thread-safe, waitable wrapper around `StateMachine`.
	 *
	 * Like `AtomicStateMachine`, but additionally keeps a monotonic transition
	 * counter (`version`) and allows callers to block until the machine reaches
	 * a state (or version) satisfying a predicate. Every successful transition
	 * bumps the version and wakes all waiters, so a waiter can detect that a
	 * transition happened even if it raced past the state it was waiting for.
	 *
	 * @tparam States Variant of state alternatives. Must match the definition's `States`.
	 * @tparam Events Variant of event alternatives. Must match the definition's `Events`.
	 * @tparam ErrorT Error type used by actions. Must match the definition's `ErrorT`.
	 */
	template<typename States, typename Events, typename ErrorT = std::string>
	class WaitableStateMachine final {
	public:
		using InnerMachine    = StateMachine<States, Events, ErrorT>;
		using StateMachineDef = typename InnerMachine::StateMachineDef;

		/**
		 * @brief State of the machine at a given point in time.
		 * `version` counts successful transitions since construction.
		 */
		struct Snapshot {
			States state;
			u64    version;
		};

		/**
		 * @brief Result of dispatching an event:
		 *  * `std::nullopt` - no transition registered for the (state, event) pair (or the
		 *    `handleEventIf` predicate rejected the current state); the machine is unchanged,
		 *  * `Snapshot` - the transition fired; holds the new state and version,
		 *  * `std::unexpected(error)` - the action reported an error; the machine is unchanged.
		 */
		using ResultT = base::Optional<std::expected<Snapshot, ErrorT>>;

		WaitableStateMachine(States initial_state, CRef<StateMachineDef> def)
			requires std::copy_constructible<States>
			  : machine(std::move(initial_state), def) {}

		WaitableStateMachine(const WaitableStateMachine&)            = delete;
		WaitableStateMachine& operator=(const WaitableStateMachine&) = delete;
		WaitableStateMachine(WaitableStateMachine&&)                 = delete;
		WaitableStateMachine& operator=(WaitableStateMachine&&)      = delete;

		/**
		 * @brief Dispatches an event to the machine under a lock.
		 * On success bumps the version and wakes all `waitUntil` waiters.
		 * See `StateMachine::handleEvent` for the dispatch semantics.
		 */
		ResultT handleEvent(const Events& event) {
			return handleEventIf([](const States&) { return true; }, event);
		}

		/**
		 * @brief Like `handleEvent`, but does not wake `waitUntil` waiters.
		 *
		 * Use when dependent state must be published elsewhere before waiters may
		 * observe this transition; call `notifyWaiters` once that is done. Waiters
		 * that poll the state concurrently still see the new state immediately.
		 */
		ResultT handleEventDeferNotify(const Events& event) {
			return handleEventImpl([](const States&) { return true; }, event, false);
		}

		/**
		 * @brief Wakes all `waitUntil` waiters. Pair with `handleEventDeferNotify`.
		 *
		 * Acquires the machine's mutex before notifying. Without it, a waiter whose
		 * predicate depends on external state published between the deferred
		 * transition and this call could evaluate the predicate, miss the
		 * notification fired in that window, and sleep forever.
		 */
		void notifyWaiters() const {
			{ std::unique_lock lock(mutex); }
			cv.notify_all();
		}

		/**
		 * @brief Dispatches an event only if `pred(current_state)` holds.
		 * The predicate is evaluated atomically with the transition, so this can be used
		 * for compare-and-set style updates (e.g. "panic only if not already terminal").
		 *
		 * @return `std::nullopt` if the predicate rejected the state or no transition is
		 *         registered; otherwise see `handleEvent`.
		 */
		template<typename Pred>
		requires std::predicate<Pred, const States&>
		ResultT handleEventIf(Pred&& pred, const Events& event) {
			return handleEventImpl(std::forward<Pred>(pred), event, true);
		}

		/**
		 * @brief Returns a copy of the current state.
		 */
		[[nodiscard]] States getStateCopy() const requires std::copy_constructible<States> {
			std::shared_lock lock(mutex);
			return machine.getState();
		}

		/**
		 * @brief Returns the current state together with the transition version.
		 * Capture a snapshot before requesting a transition, then pass its version to
		 * `waitUntil` to detect that the transition has happened.
		 */
		[[nodiscard]] Snapshot getSnapshot() const requires std::copy_constructible<States> {
			std::shared_lock lock(mutex);
			return { machine.getState(), version };
		}

		/**
		 * @brief Runs a callback over the current state under a lock.
		 * @note The reference passed to `f` is only valid for the duration of the call.
		 * @note `f` must not dispatch events to this machine.
		 */
		template<typename F>
		requires std::is_invocable_v<F, const States&>
		auto withState(F&& f) const -> decltype(auto) {
			std::shared_lock lock(mutex);
			return std::forward<F>(f)(machine.getState());
		}

		/**
		 * @brief Blocks until `pred(state, version)` returns true.
		 * The predicate is re-evaluated after every successful transition.
		 *
		 * @return Snapshot of the machine at the moment the predicate held.
		 */
		template<typename Pred>
		requires std::predicate<Pred, const States&, u64>
		Snapshot waitUntil(const Pred& pred) const requires std::copy_constructible<States> {
			std::shared_lock lock(mutex);
			cv.wait(lock, [&] { return pred(machine.getState(), version); });
			return { machine.getState(), version };
		}

	private:
		template<typename Pred>
		requires std::predicate<Pred, const States&>
		ResultT handleEventImpl(Pred&& pred, const Events& event, bool notify) {
			ResultT result;
			{
				std::unique_lock lock(mutex);
				if (!std::forward<Pred>(pred)(machine.getState())) return std::nullopt;

				auto inner_result = machine.handleEvent(event);
				if (!inner_result.has_value()) return std::nullopt;
				if (!inner_result.value().has_value())
					return std::unexpected(std::move(inner_result.value().error()));

				version++;
				result = Snapshot{ machine.getState(), version };
			}
			if (notify) cv.notify_all();
			return result;
		}

		mutable std::shared_mutex           mutex;
		mutable std::condition_variable_any cv;
		u64                                 version = 0;  ///< Number of successful transitions.
		InnerMachine                        machine;
	};
}
