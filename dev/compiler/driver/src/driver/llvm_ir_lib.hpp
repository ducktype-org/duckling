#pragma once

#include <string_view>

namespace compiler::driver {

/**
 * LLVM IR built-in library IR. Used by LLVM driver.
 */
constexpr std::string_view LLVM_IR_LIB= R"-----(

@.str = private unnamed_addr constant [4 x i8] c"%ld\00", align 1

define dso_local noundef i64 @builtin_output_i64(i64 noundef %0) local_unnamed_addr #0  {
  %2 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str, i64 noundef %0)
  %3 = sext i32 %2 to i64
  ret i64 %3
}

declare  noundef i32 @printf(ptr nocapture noundef readonly, ...) local_unnamed_addr #1

define dso_local i64 @builtin_input_i64() local_unnamed_addr #2  {
  %1 = alloca i64, align 8
  call void @llvm.lifetime.start.p0(i64 8, ptr nonnull %1) #7
  store i64 0, ptr %1, align 8
  %2 = call i32 (ptr, ...) @__isoc99_scanf(ptr noundef nonnull @.str, ptr noundef nonnull %1)
  %3 = icmp eq i32 %2, 1
  br i1 %3, label %5, label %4

4:
  call void @exit(i32 noundef 1) #8
  unreachable

5:
  %6 = load i64, ptr %1, align 8
  call void @llvm.lifetime.end.p0(i64 8, ptr nonnull %1) #7
  ret i64 %6
}

declare void @llvm.lifetime.start.p0(i64 immarg, ptr nocapture) #3

declare  noundef i32 @__isoc99_scanf(ptr nocapture noundef readonly, ...) local_unnamed_addr #1

declare  void @exit(i32 noundef) local_unnamed_addr #4

declare void @llvm.lifetime.end.p0(i64 immarg, ptr nocapture) #3

attributes #0 = { nofree nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #2 = { nounwind uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { mustprogress nocallback nofree nosync nounwind willreturn memory(argmem: readwrite) }
attributes #4 = { noreturn nounwind "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #5 = { mustprogress nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #6 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
attributes #7 = { nounwind }
attributes #8 = { noreturn nounwind }


)-----";

}