
base_path=./cases/single_large_function
for i in $(seq 1 10); do

    mkdir -p $base_path/$i
    mkdir -p $base_path/$i/duck
    mkdir -p $base_path/$i/cpp

    echo "Generating test case $i"
    python3 ../scripts/py/code_generating/main.py --duck_path $base_path/$i/duck/duck.dmf --cpp_path $base_path/$i/cpp/cpp.cpp --length 1000 --seed $i --global_symbol_count 2 --class_weight 0
done


