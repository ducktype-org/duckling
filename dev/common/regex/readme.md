Simple module that wraps `std::regex` in a way, that does
not require to include `<regex>`, as it is very slow to include
and compile.

It might also help in the future, if we wan't to use different regex lib.

