# set g++ binary:
cpp_c_path=g++-12

# Set duckc binary:
# duck_c_path=../build_no_debug/bin/duckc
duck_c_path=../build_rel/bin/duckc


# time_format="%E real, %U user, %S sys"
time_format="%E"

# iterate all folders in cases and measure compiler performance:
for case_dir in ./cases/*/; do
    echo "Running performance test for case: $case_dir"

    duck_module="$case_dir/duck"
    cpp_file="$case_dir/cpp.cpp"

    binary_output_dir="./binary_outputs/$(basename $case_dir)"
    mkdir -p $binary_output_dir

    duck_binary="$binary_output_dir/duck_binary"
    cpp_binary="$binary_output_dir/cpp_binary"


    # measure time taken to compile Duck code:
    ts=$(date +%s%N)  
    # /usr/bin/time -f "Duck compilation time: $time_format"
    duckc_command_string="$duck_c_path compile_package $duck_module -n duck --no-incremental -a $duck_binary"
    $duckc_command_string
    # echo "Executed command: $duckc_command_string"
    te=$(date +%s%N)
    elapsed=$((te - ts))
    elapsed_ms=$((elapsed / 1000000))
    echo "Duck compilation time: $(printf '%d.%03d seconds' $((elapsed_ms / 1000)) $((elapsed_ms % 1000)))"


    # measure time taken to compile C++ code:
    ts=$(date +%s%N)  
    # /usr/bin/time -f "C++ compilation time: $time_format" \
    $cpp_c_path -w -O0 -std=c++20 $cpp_file -o $cpp_binary
    te=$(date +%s%N)
    elapsed=$((te - ts))
    elapsed_ms=$((elapsed / 1000000))
    echo "C++ compilation time:  $(printf '%d.%03d seconds' $((elapsed_ms / 1000)) $((elapsed_ms % 1000)))"


    echo "-----------------------------------"
done







