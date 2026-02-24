add_subdirectory(copy-and-patch)

macro(process_to_binary [file])
    add_library(${file}.o OBJECT  ${file})
    target_compile_options(${file}.o PRIVATE 
        -fno-plt
        -fno-semantic-interposition
        )
    

endmacro()


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

