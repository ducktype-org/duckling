for i in $(seq 1 10); do
    echo "Generating test case $i"
    python3 ../scripts/py/code_generating/main.py --duck_path ./cases/generated_code_$i.duck --cpp_path ./cases/generated_code_$i.cpp --length 1000 --seed $i
done




