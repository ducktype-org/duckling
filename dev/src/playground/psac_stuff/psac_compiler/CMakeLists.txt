



add_example(psacklinger main.cpp USES libpsackling Base Init Clah Logger SystemCommand llvm_helpers)


add_example(psacklinger_test tests/psacklinger_test.cpp USES libpsackling psack_corpus Tester Base Init Clah Logger SystemCommand llvm_helpers)
