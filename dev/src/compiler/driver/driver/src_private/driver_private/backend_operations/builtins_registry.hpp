#pragma once

// Builtin LLVM bitcode blobs for different targets.
// Currently, we support only the x86_64-linux-gnu target.

extern unsigned char* builtins_x86_64_linux_gnu_bc;
extern unsigned int   builtins_x86_64_linux_gnu_bc_len;

// We may want to generate this file via CMake at some point.
// This is because a single release build cannot handle all targets,
// because some targets (such as for Apple) may require SDKs that are only
// accessible on specific hosts (such as Apple itself), requiring releases for those hosts.
