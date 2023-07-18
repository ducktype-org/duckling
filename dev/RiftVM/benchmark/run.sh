. common.sh

test_timeout=210
repeat_count=5

short_wait=1
medium_wait=5
long_wait=10

echo "Please read carefully:"

echo "1. Make sure you put correct executable paths in config.sh!"
echo

echo "2. Make sure you verified versions using verify.sh!"
echo

echo "3. Make sure you run make.sh, and it was successful!"
echo

echo "4. This script will overwrite: result/collatz.out, result/fib_iter.out, result/fib_rec.out!"
echo

echo "5. This script will wait a between test, to let computer 'cool off'."
echo

echo "6. This script might take a long of time!"
echo

echo "7. Make sure that computer is doing as little things as possible before running this script!"
echo

read -p "Continue (y/n)?" choice
case "$choice" in 
  y|Y ) ;;
  n|N ) echo "Aborting"; exit;;
  * ) echo "Invalid option"; exit;;
esac

lang_count=10

lang_descs=(" C++ " " C++ with gdb " "C++ with valgrind " " Java with JIT " " Java without Jit " " NodeJS " " NodeJS without JIT" " Python " " RiftVM " " RiftVM with debug ")


run_single() {
	header "$1"

	echo

	IS_LONG_TEST=false

	echo "Running testcase 1"
	repeat_testcase "$2 < $3 > /dev/null" $6

	sleep $short_wait
	
	echo "Running testcase 2"
	repeat_testcase "$2 < $4> /dev/null" $6
	
	sleep $medium_wait

	echo "Running testcase 3"
	IS_LONG_TEST=true
	repeat_testcase "$2 < $5 > /dev/null" $6

	sleep $long_wait

	echo >> $6

	footer
}


# PARAMETERS:
# $1 -- git name ex: "COLLATZ"
# $2 -- result file name ex: "collatz.out"
# $3 -- folder name ex: "fibonacci_iter"
# $4 -- js, py, rbc name, exe name, ex: "collatz"
# $5 -- debug exe name, ex: "collatz_debug"
# $6 -- java name, ex: "Collatz"
# $7 $8 $9 -- test files
run_full_generic() {


	# Clear file:
	echo > ./results/$2

	echo
	echo "TEST $1"
	echo

	IS_LONG_LANG=false

	run_single "${lang_descs[0]}" "$run_cpp ./$3/$4.exe" $7 $8 $9 "./results/$2"
	run_single "${lang_descs[1]}" "$run_cpp_gdb ./$3/$5.exe" $7 $8 $9 "./results/$2"
	run_single "${lang_descs[2]}" "$run_cpp_valgrind ./$3/$5.exe" $7 $8 $9 "./results/$2"

	run_single "${lang_descs[3]}" "$run_java -classpath $3 $6" $7 $8 $9 "./results/$2"
	run_single "${lang_descs[4]}" "$run_java_no_jit -classpath $3 $6" $7 $8 $9 "./results/$2"

	run_single "${lang_descs[5]}" "$run_node ./$3/$4.js" $7 $8 $9 "./results/$2"

	# IS_LONG_LANG=true
	run_single "${lang_descs[6]}" "$run_node_no_jit ./$3/$4.js" $7 $8 $9 "./results/$2"

	run_single "${lang_descs[7]}" "$run_python ./$3/$4.py" $7 $8 $9 "./results/$2"

	# IS_LONG_LANG=false
	run_single "${lang_descs[8]}" "$run_rbc ./$3/$4.rbc" $7 $8 $9 "./results/$2"
	
	# IS_LONG_LANG=true
	run_single "${lang_descs[9]}" "$run_rbc_debug ./$3/$4.rbc" $7 $8 $9 "./results/$2"
}


run_collatz_full() {
	run_full_generic \
		"COLLATZ" \
		"collatz.out" \
		"collatz" \
		"collatz" \
		"collatz_debug" \
		"Collatz" \
		"./collatz/1e5.in"    \
		"./collatz/1e6.in"    \
		"./collatz/1e7.in"
}

run_fib_iter_full() {
	run_full_generic \
		"FIB ITER" \
		"fib_iter.out" \
		"fibonacci_iter" \
		"fib_iter" \
		"fib_iter_debug" \
		"Fib_iter" \
		"./fibonacci_iter/input1.in"    \
		"./fibonacci_iter/input2.in"    \
		"./fibonacci_iter/input3.in"
}

run_fib_rec_full () {
	run_full_generic \
		"FIB REC" \
		"fib_rec.out" \
		"fibonacci_rec" \
		"fib_rec" \
		"fib_rec_debug" \
		"Fib_rec" \
		"./fibonacci_rec/input1.in"    \
		"./fibonacci_rec/input2.in"    \
		"./fibonacci_rec/input3.in"
}

run_collatz_full
run_fib_iter_full
run_fib_rec_full



