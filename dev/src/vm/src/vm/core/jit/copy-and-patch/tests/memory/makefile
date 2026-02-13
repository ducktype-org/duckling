CXXFLAGS=-std=c++26 -g3 -O3
CXX=clang++-19

data.o: data.cpp
	$(CXX) $(CXXFLAGS) -fvisibility=protected -fno-plt -fno-semantic-interposition -c $?
	
data: data.o
	$(LD) -r -Bsymbolic-functions $? -o $@

data.nm: data
	llvm-nm-19 -S --demangle --format=posix $? | awk '{print "{ .name =\"" $$1 "\", .type = \"" $$2 "\", .place = 0x" $$3 ", .size = 0x" $$4 "},"}' >$@

data-text:
	llvm-objcopy-19 -O binary --only-section=.text data $@
test: test.cpp data.nm data-text
	$(CXX) $(CXXFLAGS) test.cpp -o $@


clean:
	rm -f data.nm data.o data data-text test
