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
 *  * `Events` - an `std::variant` whose alternatives are the events that
 *    can be dispatched to the machine.
 *  * `ErrorT` - error type used by guards and actions. Defaults to
 *    `std::string`.
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
 * Three additional overloads are provided in which each action publishes the new state through a
 * `setState` callback (see `thread_safe_vm_example.cpp`):
 *  * `addAtomicTransition<State, Event>`
 *  * `addAtomicTransitions<Event, FromStates...>`
 *  * `addAtomicTransitionFromAllStates<Event>`
 *
 * Actions and guards
 * ------------------
 *
 * Each transition is composed of two callables:
 *  * `action` - executed when the transition is performed.
 *    - Standard actions return `std::expected<States, ErrorT>` - the new
 *      state on success or an error on failure.
 *    - Atomic actions take an additional `setState` callback of type
 *      `std::function<void(States)>` and return
 *      `std::expected<void, ErrorT>`. They are expected to call
 *      `setState(new_state)` when they decide to publish a new state.
 *  * `guard` (optional) - executed before the action. Returns
 *    `std::expected<void, ErrorT>` and may veto the transition by
 *    returning an error.
 *
 * If a guard returns an error the action is not executed and the machine
 * stays in its current state.
 *
 * Handling events
 * ---------------
 *
 * `StateMachine::handleEvent(event)` returns a
 * `base::Optional<std::expected<void, ErrorT>>`:
 *  * `std::nullopt` - when the machine is in a state for which the action
 * 	  is not specified as a valid transition. The machine is unchanged.
 *  * `std::expected<void, ErrorT>{}` - the transition fired successfully
 *    and the state was updated. For standard actions, the state has been
 * 	  updated to the value the action returned. For atomic actions, the state
 *	  was updated iff the action called `setState`.
 *  * `std::unexpected(error)` - the guard or the action reported an
 *    error. The machine stays in its current state.
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
#include <expected>
#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

namespace state_machine {
#define ASSERT_TRANSITION_NOT_REGISTERED(STATE_IDX, EVENT_IDX) \
	CORE_ASSERT(                                               \
		!transitions[state_idx][event_idx].has_value(),        \
		"Transition for this State (variant index: ",          \
		state_idx,                                             \
		") and Event (variant index: ",                        \
		event_idx,                                             \
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
	 * @tparam ErrorT Error type used by guards and actions.
	 */
	template<typename States, typename Events, typename ErrorT = std::string>
	requires base::IsVariant<States> && base::IsVariant<Events> class StateMachineDefinition {
	public:
		friend class StateMachine<States, Events, ErrorT>;
		using ActionResultT       = std::expected<States, ErrorT>;
		using AtomicActionResultT = std::expected<void, ErrorT>;
		using GuardResultT        = std::expected<void, ErrorT>;

		/**
		 * @brief Callback type handed to atomic actions so they can
		 * publish a new state into the owning `StateMachine`. This function is expected to call
		 * `setState()` when updating the new state.
		 */
		using SetStateCallback = std::function<void(States)>;

	private:
		// Type-erased handlers, so they can all be stored in the same array.
		using RawAction       = std::function<ActionResultT(const States&, const Events&)>;
		using RawAtomicAction = std::function<
			AtomicActionResultT(const States&, const Events&, const SetStateCallback&)>;
		using RawGuard = std::function<GuardResultT(const States&, const Events&)>;

		template<typename Event>
		using GenericAction = std::function<ActionResultT(const States&, const Event&)>;
		template<typename Event>
		using GenericAtomicAction
			= std::function<AtomicActionResultT(const States&, const Event&, const SetStateCallback&)>;
		template<typename Event>
		using GenericGuard = std::function<GuardResultT(const States&, const Event&)>;

		/**
		 * @brief Represents a single transition entry for a (State, Event) pair.
		 */
		struct TransitionEntry {
			std::variant<RawAction, RawAtomicAction>
				action;  ///< either a standard action that returns the new state, or an atomic action
			             ///< that publishes the new state via a `setState` callback handed to it.
			base::Optional<RawGuard>
				guard;  ///< Optional guard to additionally validate if the transition is allowed.
		};

		static constexpr usize NUM_STATES = std::variant_size_v<States>;
		static constexpr usize NUM_EVENTS = std::variant_size_v<Events>;
		/**
		 * @brief An allowed transitions map. For each (State, Event) pair stores the optional
		 * (Action, Guard) pair to be performed on transition. Empty optional means that this
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
		 * @param guard  Optional callable evaluated before `action`. May
		 *               veto the transition by returning
		 *               `std::unexpected(error)`.
		 */
		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		void addTransition(
			std::function<ActionResultT(const State&, const Event&)>                action,
			base::Optional<std::function<GuardResultT(const State&, const Event&)>> guard = {}
		) {
			const usize state_idx = base::variantTypeIndex<States, State>();
			const usize event_idx = base::variantTypeIndex<Events, Event>();
			ASSERT_TRANSITION_NOT_REGISTERED(state_idx, event_idx);

			RawAction raw_action
				= [action = std::move(action)](const States& s, const Events& e) -> ActionResultT {
				return action(std::get<State>(s), std::get<Event>(e));
			};

			base::Optional<RawGuard> raw_guard{};
			if (guard.has_value()) {
				raw_guard =
					[guard = std::move(*guard)](const States& s, const Events& e) -> GuardResultT {
					return guard(std::get<State>(s), std::get<Event>(e));
				};
			}

			transitions.at(state_idx).at(event_idx)
				= TransitionEntry{ std::move(raw_action), std::move(raw_guard) };
		}

		/**
		 * @brief Registers the same transition for several source states.
		 *
		 * Useful when the same `(action, guard)` should fire from any of
		 * several states.
		 *
		 * @tparam Event      Triggering event type. Must be an alternative
		 *                    of `Events`.
		 * @tparam FromStates Source states from which the event is
		 *                    accepted. Each must be an alternative of `States`.
		 */
		template<typename Event, typename... FromStates>
		requires(base::IsVariantMember<FromStates, States> && ...)
		     && base::IsVariantMember<Event, Events>
		void addTransitions(
			const GenericAction<Event>& action, const base::Optional<GenericGuard<Event>>& guard = {}
		) {
			(addTransitionInternal<FromStates, Event>(action, guard), ...);
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
		requires base::IsVariantMember<Event, Events> void addTransitionFromAllStates(
			const GenericAction<Event>& action, const base::Optional<GenericGuard<Event>>& guard = {}
		) {
			[this, &action, &guard]<usize... Is>(std::index_sequence<Is...>) {
				(this->addTransitionInternal<std::variant_alternative_t<Is, States>, Event>(
					 action, guard
				 ),
				 ...);
			}(std::make_index_sequence<NUM_STATES>{});
		}

		// =========================================================
		// Atomic transitions
		// =========================================================

		/**
		 * @brief Atomic counterpart for `addTransition`. See its docs for more info.
		 */
		template<typename State, typename Event>
		requires base::IsVariantMember<State, States> && base::IsVariantMember<Event, Events>
		void addAtomicTransition(
			std::function<AtomicActionResultT(const State&, const Event&, const SetStateCallback&)>
																					action,
			base::Optional<std::function<GuardResultT(const State&, const Event&)>> guard = {}
		) {
			const usize state_idx = base::variantTypeIndex<States, State>();
			const usize event_idx = base::variantTypeIndex<Events, Event>();
			ASSERT_TRANSITION_NOT_REGISTERED(state_idx, event_idx);

			RawAtomicAction raw_action
				= [action = std::move(action)](
					  const States& s, const Events& e, const SetStateCallback& set_state
				  ) -> AtomicActionResultT {
				return action(std::get<State>(s), std::get<Event>(e), set_state);
			};

			base::Optional<RawGuard> raw_guard{};
			if (guard.has_value()) {
				raw_guard =
					[guard = std::move(*guard)](const States& s, const Events& e) -> GuardResultT {
					return guard(std::get<State>(s), std::get<Event>(e));
				};
			}

			transitions.at(state_idx).at(event_idx)
				= TransitionEntry{ std::move(raw_action), std::move(raw_guard) };
		}

		/**
		 * @brief Atomic counter part of `addTransitions`. See its docs for more info.
		 */
		template<typename Event, typename... FromStates>
		requires(base::IsVariantMember<FromStates, States> && ...)
		     && base::IsVariantMember<Event, Events>
		void addAtomicTransitions(
			const GenericAtomicAction<Event>&          action,
			const base::Optional<GenericGuard<Event>>& guard = {}
		) {
			(addAtomicTransitionInternal<FromStates, Event>(action, guard), ...);
		}

		/**
		 * @brief Atomic counter part of `addTransitionFromEveryState`. See its docs for more info.
		 */
		template<typename Event>
		requires base::IsVariantMember<Event, Events> void addAtomicTransitionFromAllStates(
			const GenericAtomicAction<Event>&          action,
			const base::Optional<GenericGuard<Event>>& guard = {}
		) {
			[this, &action, &guard]<usize... Is>(std::index_sequence<Is...>) {
				(this->addAtomicTransitionInternal<std::variant_alternative_t<Is, States>, Event>(
					 action, guard
				 ),
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
		void addTransitionInternal(
			const GenericAction<Event>& action, const base::Optional<GenericGuard<Event>> guard
		) {
			const usize state_idx = base::variantTypeIndex<States, State>();
			const usize event_idx = base::variantTypeIndex<Events, Event>();
			ASSERT_TRANSITION_NOT_REGISTERED(state_idx, event_idx);

			// Wrapper over action so it can be stored next to strongly-typed actions.
			RawAction raw_action = [action](const States& s, const Events& e) {
				return action(s, std::get<Event>(e));
			};

			// Wrapper over guard so it can be stored next to strongly-typed guards.
			base::Optional<RawGuard> raw_guard = std::nullopt;
			if (guard.has_value()) {
				raw_guard = [guard = *guard](const States& s, const Events& e) -> GuardResultT {
					return guard(std::get<State>(s), std::get<Event>(e));
				};
			}

			transitions.at(state_idx).at(event_idx)
				= TransitionEntry{ std::move(raw_action), std::move(raw_guard) };
		}

		/**
		 * @brief Atomic counterpart of `addTransitionInternal`.
		 *
		 * Shared logic used by `addAtomicTransitions` and
		 * `addAtomicTransitionFromAllStates`.
		 */
		template<typename State, typename Event>
		void addAtomicTransitionInternal(
			const GenericAtomicAction<Event>& action, const base::Optional<GenericGuard<Event>> guard
		) {
			const usize state_idx = base::variantTypeIndex<States, State>();
			const usize event_idx = base::variantTypeIndex<Events, Event>();
			ASSERT_TRANSITION_NOT_REGISTERED(state_idx, event_idx);

			// Wrapper over action so it can be stored next to strongly-typed actions.
			RawAtomicAction raw_action
				= [action](
					  const States& s, const Events& e, const SetStateCallback& set_state
				  ) -> AtomicActionResultT { return action(s, std::get<Event>(e), set_state); };

			// Wrapper over guard so it can be stored next to strongly-typed guards.
			base::Optional<RawGuard> raw_guard = std::nullopt;
			if (guard.has_value()) {
				raw_guard = [guard = *guard](const States& s, const Events& e) -> GuardResultT {
					return guard(std::get<State>(s), std::get<Event>(e));
				};
			}

			transitions.at(state_idx).at(event_idx)
				= TransitionEntry{ std::move(raw_action), std::move(raw_guard) };
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
	 * @tparam States Variant of state alternatives. Must match the
	 *                definition's `States`.
	 * @tparam Events Variant of event alternatives. Must match the
	 *                definition's `Events`.
	 * @tparam ErrorT Error type used by guards and actions. Must match
	 *                the definition's `ErrorT`.
	 */
	template<typename States, typename Events, typename ErrorT = std::string>
	class StateMachine {
	public:
		using ResultT          = base::Optional<std::expected<void, ErrorT>>;
		using StateMachineDef  = StateMachineDefinition<States, Events, ErrorT>;
		using SetStateCallback = typename StateMachineDef::SetStateCallback;
		using RawAction        = typename StateMachineDef::RawAction;
		using RawAtomicAction  = typename StateMachineDef::RawAtomicAction;

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
		 *  1. evaluates the guard if present; on error returns
		 *     `std::unexpected(error)` and does not change state,
		 *  2. evaluates the action; on error returns
		 *     `std::unexpected(error)` and does not change state,
		 *  3. on success moves the new state into the machine and returns
		 *     `std::expected<void, ErrorT>{}`.
		 *
		 * @return * `std::nullopt` if no transition is registered for the
		 *           current `(state, event)` pair.
		 *         * `std::expected<void, ErrorT>{}` if the transition
		 *           fired successfully.
		 *         * `std::unexpected(error)` if the guard or the action
		 *           reported an error. The state is not changed.
		 */
		ResultT handleEvent(const Events& event) {
			return std::visit(
				[&](auto&& inner_state, auto&& inner_event) -> ResultT {
					using State           = std::decay_t<decltype(inner_state)>;
					using Event           = std::decay_t<decltype(inner_event)>;
					auto maybe_transition = definition->template getTransition<State, Event>();
					if (!maybe_transition.has_value()) return std::nullopt;

					const auto& transition = maybe_transition.value();

					// Check the guard first, if one was registered.
					if (transition->guard.has_value()) {
						auto guard_result = (*transition->guard)(current_state, event);
						if (!guard_result.has_value())
							return std::unexpected(std::move(guard_result.error()));
					}

					variant_match(transition->action) {
						variant_case(RawAction, raw_action) {
							// Standard action. Action returns ehe new state.
							auto action_result = raw_action(current_state, event);
							if (!action_result.has_value())
								return std::unexpected(std::move(action_result.error()));

							current_state = std::move(action_result.value());
							return std::expected<void, ErrorT>{};
						}
						variant_case(RawAtomicAction, atomic_action) {
							// Atomic action. Save the previous state so it remains valid for the
						    // action body after setState has changed the current state.
							States           prev_state = current_state;
							SetStateCallback set_state  = [this](States new_state) {
                                current_state = std::move(new_state);
							};
							auto action_result = atomic_action(prev_state, event, set_state);
							if (!action_result.has_value())
								return std::unexpected(std::move(action_result.error()));
							return std::expected<void, ErrorT>{};
						}
						variant_default { CORE_UNREACHABLE(); }
					}
				},
				current_state,
				event
			);
		}

		/**
		 * @brief Returns a CRef to the current state.
		 */
		[[nodiscard]] const States& getState() const { return current_state; }

	private:
		States current_state;              ///< Current state of the machine.
		CRef<StateMachineDef> definition;  ///< The definition the state machine was configured with.
	};

}
