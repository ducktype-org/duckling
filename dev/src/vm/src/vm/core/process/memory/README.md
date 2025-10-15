# DVM - Safe Memory Module
## Block
- Explain how the block is a handle for the data
- Explain the nested structure
- Reference counting
- Variant type to justify the structure
- It knows the type which it holds
### Nested block structure
- How it works and why

## Pointer
- A fat pointer of size 16
- Stores the block it looks on and the offset in the block
- Null pointers are represented by the MRef<Block> set to null



## Memory module
- One memory module per process
- Memory module governs the pointers and all the data usages in the runtime
- Handles creating block, destroying them, invokes copy constructors and data destructors when data is moved around

- Validates the memory state at the end of execution. Look for any unfreed blocks etc. 
- Thanks to the memory module we can detect memory leaks, use after frees etc.
- Updates refcounts when data is moved 

# DVM - Fast memory module
- No blocks and mutexes when reaching through them
- 8byte pointers to allow for ffi
- No safety guarantees

// TODOP: Link to appropriate files