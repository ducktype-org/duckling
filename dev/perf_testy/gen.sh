for i in $(seq 1 10); do


    mkdir ./cases/$i
    mkdir ./cases/$i/duck

    echo "Generating test case $i"
    python3 ../scripts/py/code_generating/main.py --duck_path ./cases/$i/duck/duck.dmf --cpp_path ./cases/$i/cpp.cpp --length 1000 --seed $i
done




