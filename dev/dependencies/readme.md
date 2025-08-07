# Dependencies

Dependencies is a folder that stores `CMakeLists.txt` file that includes all dependecies.
Each dependency should be stored in separate .cmake file. Each file should contain code handling dependency,
it will be included in local  or main `CMakeLists.txt` file.
The standard behavior is to place dependencies inside build directory.
