. config.sh

echo "JavaScript skipped"
echo "Python skipped"
echo "RiftVM skipped"
echo 

echo "Java begin"
$javac_bin ./collatz/Collatz.java
$javac_bin ./fibonacci_iter/Fib_iter.java
$javac_bin ./fibonacci_rec/Fib_rec.java
echo "Java done"

echo "C++ begin"
$cpp_bin -O2 ./collatz/collatz.cpp -o ./collatz/collatz.exe
$cpp_bin -O0 -g ./collatz/collatz.cpp -o ./collatz/collatz_debug.exe

$cpp_bin -O2 ./fibonacci_iter/fib_iter.cpp -o ./fibonacci_iter/fib_iter.exe
$cpp_bin -O0 -g ./fibonacci_iter/fib_iter.cpp -o ./fibonacci_iter/fib_iter_debug.exe

$cpp_bin -O2 ./fibonacci_rec/fib_rec.cpp -o ./fibonacci_rec/fib_rec.exe
$cpp_bin -O0 -g ./fibonacci_rec/fib_rec.cpp -o ./fibonacci_rec/fib_rec_debug.exe


echo "C++ done"