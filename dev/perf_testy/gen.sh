for i in $(seq 1 10); do

    mkdir ./cases/$i

    echo "Generating test case $i"
    python3 ../scripts/py/code_generating/main.py --duck_path ./cases/$i/duck.dmf --cpp_path ./cases/$i/cpp.cpp --length 8000 --seed $i
done




