

linker input
- object files - some objects are from our compilation, some objects are our libraries that may be compiled differently (form lir for example) 
- maybe asm files (?)
- external libraries (static)


```
clang -Wno-unused-command-line-argument "/tmp/mine-runtime-7dc23bb008f0.o" "/tmp/mine-mem-7dc23bb02ff0.o" "/tmp/mine-bytes-7dc23bb032f0.o" "/tmp/mine-io-7dc23bb01df0.o" "/tmp/mine-compress_zlib-7dc23bb04af0.o" "/tmp/mine-math-7dc23bb05cf0.o" "/tmp/mine-strconv_decimal-7dc23bb047f0.o" "/tmp/mine-strconv-7dc23bb026f0.o" "/tmp/mine-image-7dc23bb020f0.o" "/tmp/mine-time-7dc23bb023f0.o" "/tmp/mine-utf8-7dc23bb02cf0.o" "/tmp/mine-bufio-7dc23bb044f0.o" "/tmp/mine-hash-7dc23bb059f0.o" "/tmp/mine-png-7dc23bb01af0.o" "/tmp/mine-reflect-7dc23bb062f0.o" "/tmp/mine-fmt-7dc23bb014f0.o" "/tmp/mine-7dc23bb00570.o" "/tmp/mine-strings-7dc23bb011f0.o" "/tmp/mine-unicode-7dc23bb038f0.o" "/tmp/mine-slashpath-7dc23bb017f0.o" "/tmp/mine-os-7dc23bb00ef0.o" "/tmp/mine-linux-7dc23bb029f0.o" "/tmp/mine-generate_image_info-7dc23bb00bf0.o" "/tmp/mine-utf16-7dc23bb03bf0.o" "/tmp/mine-compress-7dc23bb056f0.o" "/tmp/mine-unix-7dc23bb041f0.o"  -o "/home/wojtek/ducktype/pl/odin/compiler/Odin/mine.bin"  -lm -lc   -L/       -no-pie -Wl,-rpath,\$ORIGIN  
```

Each file is a different library that is compiled from source code and uses FFI function calls to standard library.

Compiling module 1
Compiling module 2
Compiling module 3
---> linking
---> executables