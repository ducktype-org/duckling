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

@stdin = external local_unnamed_addr global ptr, align 8
@stdout = external local_unnamed_addr global ptr, align 8
@.str.format.output.i64 = private unnamed_addr constant [5 x i8] c"%ld\0A\00", align 1
@.str.format.input.i64 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@.str.format.output.u64 = private unnamed_addr constant [5 x i8] c"%lu\0A\00", align 1
@.str.format.input.u64 = private unnamed_addr constant [4 x i8] c"%lu\00", align 1
@.str.format.output.f64 = private unnamed_addr constant [5 x i8] c"%lf\0A\00", align 1
@.str.format.input.f64 = private unnamed_addr constant [4 x i8] c"%lf\00", align 1
@.str.format.output.str = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@.str.format.input.str = private unnamed_addr constant [3 x i8] c"%s\00", align 1

declare noundef i32 @printf(ptr nocapture noundef readonly, ...)
declare noundef i32 @__isoc99_scanf(ptr nocapture noundef readonly, ...)
declare noundef i64 @fwrite(ptr noundef readonly nocapture, i64 noundef, i64 noundef, ptr noundef nocapture)
declare void @llvm.memset.p0.i64(ptr writeonly nocapture, i8, i64, i1 immarg)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias writeonly nocapture, ptr noalias readonly nocapture, i64, i1 immarg)
declare i64 @__getdelim(ptr noundef, ptr noundef, i32 noundef, ptr noundef)
declare noundef i32 @putchar(i32 noundef)
declare noalias noundef ptr @malloc(i64 noundef)
declare void @free(ptr allocptr noundef nocapture)
declare void @exit(i32 noundef)





define noundef i32 @builtin_output_i64(i64 noundef %0) {
  %2 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.format.output.i64, i64 noundef %0)
  ret i32 %2
}

define noundef i64 @builtin_input_i64() {
  %1 = alloca i64, align 8
  %2 = call i32 (ptr, ...) @__isoc99_scanf(ptr noundef nonnull @.str.format.input.i64, ptr noundef nonnull %1)
  %3 = icmp eq i32 %2, 1
  br i1 %3, label %5, label %4

4:
  call void @exit(i32 noundef 1)
  unreachable

5:
  %6 = load i64, ptr %1, align 8
  ret i64 %6
}

define noundef i32 @builtin_output_u64(i64 noundef %0) {
  %2 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.format.output.u64, i64 noundef %0)
  ret i32 %2
}

define noundef i64 @builtin_input_u64() {
  %1 = alloca i64, align 8
  %2 = call i32 (ptr, ...) @__isoc99_scanf(ptr noundef nonnull @.str.format.input.u64, ptr noundef nonnull %1)
  %3 = icmp eq i32 %2, 1
  br i1 %3, label %5, label %4

4:
  call void @exit(i32 noundef 1) #5
  unreachable

5:
  %6 = load i64, ptr %1, align 8
  ret i64 %6
}

define noundef i32 @builtin_output_f64(double noundef %0) {
  %2 = tail call i32 (ptr, ...) @printf(ptr noundef nonnull dereferenceable(1) @.str.format.output.f64, double noundef %0)
  ret i32 %2
}

define noundef double @builtin_input_f64() {
  %1 = alloca double, align 8
  %2 = call i32 (ptr, ...) @__isoc99_scanf(ptr noundef nonnull @.str.format.input.f64, ptr noundef nonnull %1)
  %3 = icmp eq i32 %2, 1
  br i1 %3, label %5, label %4

4:
  call void @exit(i32 noundef 1)
  unreachable

5:
  %6 = load double, ptr %1, align 8
  ret double %6
}





%struct.DucklingString = type { ptr, i64, i64, i64 }

define noundef i64 @builtin_output_string(ptr noundef readonly byval(%struct.DucklingString) align 8 nocapture %0) {
  %2 = load ptr, ptr %0, align 8
  %3 = icmp eq ptr %2, null
  %4 = getelementptr inbounds nuw i8, ptr %0, i64 8
  %5 = load i64, ptr %4, align 8
  %6 = icmp eq i64 %5, 0
  %7 = select i1 %3, i1 true, i1 %6
  br i1 %7, label %11, label %8

8:
  %9 = load ptr, ptr @stdout, align 8
  %10 = tail call i64 @fwrite(ptr noundef nonnull %2, i64 noundef 1, i64 noundef %5, ptr noundef %9)
  br label %11

11:
  %12 = phi i64 [ %10, %8 ], [ 0, %1 ]
  %13 = tail call i32 @putchar(i32 10)
  ret i64 %12
}

define void @builtin_input_string(ptr dead_on_unwind noalias writable writeonly sret(%struct.DucklingString) align 8 nocapture %0) {
  %2 = alloca ptr, align 8
  %3 = alloca i64, align 8
  store ptr null, ptr %2, align 8
  store i64 0, ptr %3, align 8
  %4 = load ptr, ptr @stdin, align 8
  %5 = call i64 @__getdelim(ptr noundef nonnull %2, ptr noundef nonnull %3, i32 noundef 10, ptr noundef %4)
  %6 = icmp eq i64 %5, -1
  br i1 %6, label %7, label %9

7:
  %8 = load ptr, ptr %2, align 8
  call void @free(ptr noundef %8)
  call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(32) %0, i8 0, i64 32, i1 false)
  br label %29

9:
  %10 = icmp sgt i64 %5, 0
  br i1 %10, label %11, label %19

11:
  %12 = load ptr, ptr %2, align 8
  %13 = getelementptr i8, ptr %12, i64 %5
  %14 = getelementptr i8, ptr %13, i64 -1
  %15 = load i8, ptr %14, align 1
  %16 = icmp eq i8 %15, 10
  br i1 %16, label %17, label %19

17:
  store i8 0, ptr %14, align 1
  %18 = add nsw i64 %5, -1
  br label %19

19:
  %20 = phi i64 [ %18, %17 ], [ %5, %11 ], [ %5, %9 ]
  %21 = call noalias ptr @malloc(i64 noundef %20)
  %22 = icmp eq ptr %21, null
  %23 = load ptr, ptr %2, align 8
  br i1 %22, label %24, label %25

24:
  call void @free(ptr noundef %23)
  call void @exit(i32 noundef 1)
  unreachable

25:
  call void @llvm.memcpy.p0.p0.i64(ptr nonnull align 1 %21, ptr align 1 %23, i64 %20, i1 false)
  call void @free(ptr noundef %23)
  store ptr %21, ptr %0, align 8
  %26 = getelementptr inbounds nuw i8, ptr %0, i64 8
  store i64 %20, ptr %26, align 8
  %27 = getelementptr inbounds nuw i8, ptr %0, i64 16
  store i64 0, ptr %27, align 8
  %28 = getelementptr inbounds nuw i8, ptr %0, i64 24
  store i64 %20, ptr %28, align 8
  br label %29

29:
  ret void
}

define void @builtin_free_string(ptr noundef nonnull align 8 nocapture dereferenceable(32) %0) {
  %2 = load ptr, ptr %0, align 8
  %3 = icmp eq ptr %2, null
  br i1 %3, label %9, label %4

4:
  %5 = getelementptr inbounds nuw i8, ptr %0, i64 16
  %6 = load i64, ptr %5, align 8
  %7 = sub i64 0, %6
  %8 = getelementptr inbounds i8, ptr %2, i64 %7
  tail call void @free(ptr noundef %8)
  tail call void @llvm.memset.p0.i64(ptr noundef nonnull align 8 dereferenceable(32) %0, i8 0, i64 32, i1 false)
  br label %9

9:
  ret void
}

)-----";

}
