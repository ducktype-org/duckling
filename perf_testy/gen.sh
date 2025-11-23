# iterate from 1 to 1000:
for i in $(seq 1 1000); do
    echo "Generating test case of length $i"
    python3 ../dev/scripts/py/code_generating/main.py --duck_path ./cases/generated_code_$i.duck --cpp_path ./cases/generated_code_$i.cpp --length 1000 --seed 42
done




