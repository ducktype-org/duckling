# run g++ for c++ cases and test its time:

for i in $(seq 1 10); do
    echo "Running test case $i"
    g++ -O0 -o ./cases/generated_code_$i ./cases/generated_code_$i.cpp
    /usr/bin/time -f "C++ Time: %E, Memory: %M KB" ./cases/generated_code_$i < ./cases/generated_code_$i.duck > /dev/null
done
