# check if the number of arguments is 1:
if [ $# -ne 1 ]; then
	echo "Error: Number of arguments must be 1"
	exit 1
fi

# run duck and cpp on input parameter:

count_cpp=$(./bin/count_tokens_cpp $1.cpp)
count_duck=$(./bin/count_tokens_duckling $1.duck)

echo "C++: $count_cpp"
echo "Duck: $count_duck"

percentage_diff=`echo "scale=2; $count_duck / $count_cpp * 100" | bc`
echo "Duck percentage of C++: $percentage_diff%"

