# File names and directories
 
  * Module and submodule tests should be placed in `module-path/tests/` folder.
  * Additional test files and subfolder should be placed `tests` folder.
  * Example files should be placed in `module-path/examples/` folder.
  * Non-module tests should be placed inside `tests/descriptive-group-name/` folder or `tests/descriptive-group-name.cpp` file.
  * Test executables name should be `module-name_test_optional-descriptive-words`
  * Example executables name should be `module-name_example_optional-descriptive-words`

# Code

  * All tests should be written using framework provided by `tester` module and follow structure of [test_example](../../../../common/tester/examples/example.cpp)

  * All tests and examples should be added in CMake according to [CMakeGuidelines.md](CMakeGuidelines.md)

# CMake

  * Test and examples should only manually create executables, and then pass them to `rift_add_test` or `add_example` functions. 

# Additional executables guidelines

  * Each example should produce separate executable compiled
  * Tests for each module should produce single executable running all module tests unless it reasonable to split them into multiple programs
