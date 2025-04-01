#pragma once

#include <string_view>

namespace compiler::driver {

	/**
	 * LLVM IR built-in library IR. Used by LLVM driver.
   * It is based on simplified clang output of C file.
   * In the future we will probably autogenerate this from C implementation.
   * In particular this might change between systems.
	 */
	constexpr std::string_view LLVM_IR_LIB = R"-----(

@.str = private unnamed_addr constant [6 x i8] c"%ld \0A\00", align 1
@.str.1 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1

define noundef i64 @builtin_output_i64(i64 noundef %0) {
  %2 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i64 noundef %0)
  %3 = sext i32 %2 to i64
  ret i64 %3
}

declare  noundef i32 @printf(ptr nocapture noundef readonly, ...) 

define i64 @builtin_input_i64() {
  %1 = alloca i64, align 8
  store i64 0, ptr %1, align 8
  %2 = call i32 (ptr, ...) @__isoc99_scanf(ptr noundef nonnull @.str.1, ptr noundef nonnull %1)
  %3 = icmp eq i32 %2, 1
  br i1 %3, label %5, label %4

4:
  call void @exit(i32 noundef 1) 
  unreachable

5:
  %6 = load i64, ptr %1, align 8
  ret i64 %6
}

declare  noundef i32 @__isoc99_scanf(ptr nocapture noundef readonly, ...) 

declare  void @exit(i32 noundef) 

)-----";

}
