# State Machine
Simple library for building generic, type-safe finite state machines on
top of `std::variant`.

States and events are described as `std::variant`s. Allowed transitions are
registered up-front on a `StateMachineDefinition`. A `StateMachine` then
carries the runtime state and dispatches incoming events through that
definition.

For more info, look at `state_machine.hpp` docs.
