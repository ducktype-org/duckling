base_path=./cases/single_large_function
for i in $(seq 1 10); do

    mkdir -p $base_path/$i

    echo "Generating test case $i"
    python3 ../scripts/py/code_generating/main.py --duck_path $base_path/$i/duck.dmf --cpp_path $base_path/$i/cpp.cpp --length 1000 --seed $i --global_symbol_count 2
done

base_path=./cases/a_lot_of_small_functions_1000
for i in $(seq 1 10); do

    mkdir -p $base_path/$i

    echo "Generating test case $i"
    python3 ../scripts/py/code_generating/main.py --duck_path $base_path/$i/duck.dmf --cpp_path $base_path/$i/cpp.cpp --length 1000 --seed $i --global_symbol_count 80
done




base_path=./cases/a_lot_of_small_functions_8000
for i in $(seq 1 10); do

    mkdir -p $base_path/$i

    echo "Generating test case $i"
    python3 ../scripts/py/code_generating/main.py --duck_path $base_path/$i/duck.dmf --cpp_path $base_path/$i/cpp.cpp --length 8000 --seed $i --global_symbol_count 400
done




