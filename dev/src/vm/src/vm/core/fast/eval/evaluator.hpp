#pragma once
#include <vm/core/fast/program/instructions/executable.hpp>

namespace vm::fast {
    class FastExecutor {
        [[clang::always_inline]] constexpr int
eval(const vm::fast::exec::Instruction *instructions) noexcept {
  vm::Memory state;
  vm::Frame *current_frame = state.frames_begin;
  *current_frame = vm::Frame{
      .base_pointer = state.memory_begin,
      .stack = state.memory_begin,
      .instruction_pointer = instructions,
      .flag = false,
  };
  tail_call(state, current_frame);
//   bool run = true;
//   while (run) {
//     switch (current_frame->instruction_pointer->id) {
// #define HANDLE_INSTR(NAME)                                                     \
//   case vm::instr::InstrID::NAME: {                                             \
//     if constexpr (std::string_view(#NAME) != "exit") {                         \
//       vm::instr_func::NAME(state, current_frame,                               \
//                            current_frame->instruction_pointer->instr_##NAME);  \
//     } else {                                                                   \
//       run = false;                                                             \
//     }                                                                          \
//     current_frame->instruction_pointer++;                                      \
//     break;                                                                     \
//   }
// #include "vm/instructions/instruction_definitions.hpp"
// #undef HANDLE_INSTR
//     default:
//       state.error_message = "Invalid instruction ID.";
//       return 1;
//       break;
//     }
//   }
  if (state.error_message) {
    std::cerr << "Error: " << *state.error_message << "\n";
    return 1;
  }
  if (current_frame->stack != current_frame->base_pointer) {
    std::cerr << "Error: Stack not empty at the end of execution.\n";
    return 1;
  }
  return 0;
}
    };
}
