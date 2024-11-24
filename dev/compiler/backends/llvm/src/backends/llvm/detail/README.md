This directory is intended as a place for files
that include and use llvm api directly.

"Leaking" llvm headers outside this directory
is not desired, for following reasons:

* They are (really) large after all recursive includes get included.
  This has significant impact on compilation time, but more importantly on responsiveness of 
  tools like clangd (a good few second of delay between keystroke, and clangd reaction). 

* We don't want to rely directly on LLVM api in a large portion of the project, since it can change,
  when we migrate to newer LLVM versions. It should be restricted strictly this folder if possible.

