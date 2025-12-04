# get args:
result_csv=$1

case_pattern=$2

if [ -z "$result_csv" ]; then
    echo "No result CSV file provided!"
    exit 1
fi



# set g++ binary:
cpp_c_path=g++-12

# Set duckc binary:
# duck_c_path=../build_no_debug/bin/duckc
duck_c_path=../build_rel/bin/duckc


# time_format="%E real, %U user, %S sys"
time_format="%E"

# variable to collect data in csv format
csv_collect=""
csv_collect="Category,Test Case,Duck Compilation Time (ms),C++ Compilation Time (ms)\n"

# c++ setup:
cpp_flags="-std=c++20 -w -O0"
$cpp_c_path $cpp_flags -c ./cpp_setup/print.cpp -o ./cpp_setup/print.o
cpp_print_lib="./cpp_setup/print.o"


# iterate all categories in cases:
for category_dir in ./cases/$case_pattern/; do
    [ -d "$category_dir" ] || continue

    for case_dir in "$category_dir"*/; do
        [ -d "$case_dir" ] || continue
        
        category="$(basename "$category_dir")"
        case_name="$(basename "$case_dir")"

        echo "Running performance test for category: $category, case: $case_name"!


        duck_module="$case_dir/duck/"
        cpp_files="$case_dir/cpp/"

        binary_output_dir="./binary_outputs/$category/$case_name"
        mkdir -p $binary_output_dir

        duck_binary="$binary_output_dir/duck_binary"
        cpp_binary="$binary_output_dir/cpp_binary"

        csv_collect+="$category,$case_name,"


        # measure time taken to compile Duck code:
        ts=$(date +%s%N)  
        # /usr/bin/time -f "Duck compilation time: $time_format"
        duckc_command_string="$duck_c_path compile_package $duck_module -n duck --no-incremental -a $duck_binary"
        $duckc_command_string
        echo "Executed command: $duckc_command_string"
        te=$(date +%s%N)
        elapsed=$((te - ts))
        elapsed_ms=$((elapsed / 1000000))
        echo "Duck compilation time: $(printf '%d.%03d seconds' $((elapsed_ms / 1000)) $((elapsed_ms % 1000)))"

        csv_collect+="$elapsed_ms,"

        # measure time taken to compile C++ code:
        ts=$(date +%s%N)  
        # /usr/bin/time -f "C++ compilation time: $time_format" \
        $cpp_c_path $cpp_flags $cpp_files/*.cpp $cpp_print_lib -o $cpp_binary
        te=$(date +%s%N)
        elapsed=$((te - ts))
        elapsed_ms=$((elapsed / 1000000))
        echo "C++ compilation time:  $(printf '%d.%03d seconds' $((elapsed_ms / 1000)) $((elapsed_ms % 1000)))"

        csv_collect+="$elapsed_ms\n"

        echo "-----------------------------------"
    done
done

# print results and exit to avoid running the old loop below (if present)
echo -e "\nPerformance Test Results (in CSV format):"
echo -e "$csv_collect"

echo -e "$csv_collect" > "$result_csv"
echo -e "Results saved to $result_csv!"

exit 0







