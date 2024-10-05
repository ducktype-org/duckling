# check if the number of arguments is 1:
if [ $# -ne 1 ]; then
	echo "Error: Number of arguments must be 1"
	exit 1
fi

# run duck and cpp on input parameter:

echo "C++./bin/count_tokens_cpp :"
./bin/count_tokens_cpp  $1.cpp

echo "Duck:"
./bin/count_tokens_duckling $1.duck