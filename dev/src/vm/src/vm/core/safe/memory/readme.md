# DVM — Memory module

## Safe memory module

The memory module is the central authority responsible for all data allocation, access, and
lifetime management within a `VMProcess`. It is to provide safety guarantees, preventing common
memory errors like use-after-free, buffer overflows, and memory leaks. This safety is built upon
a three-tiered system: the [`Pointer`](./pointer.hpp), the [`Block`](./block.hpp), and the
[`Memory`](./memory.hpp) governor.

### [`Pointer`](./pointer.hpp)

In this VM, a pointer is not a raw memory address. Instead, it's a "fat pointer" — a 16-byte smart
handle that provides safe, managed access to data.

*   **Structure:** A `Pointer` consists of two parts:
    1.  A reference to a `Block` (`MRef<Block>`).
    2.  A 64-bit `offset` specifying a byte position within that block's data region.
*   **Null Pointers:** A pointer is considered `null` simply if its internal `Block` reference is `nullptr`.
*   **Governed Access:** A `Pointer` itself contains minimal logic. All operations on pointers 
    (dereferencing, creating, destroying copying references) are performed through static methods 
    in the `Memory` class. This ensures that every single memory access can be checked for validity 
    (e.g., is the block deallocated? is the offset in bounds?).

### [`Block`](./block.hpp)

A `Block` is the fundamental unit of memory ownership. It is not the raw data itself, but rather a 
metadata-rich "handle" that describes a region of memory. Every piece of data allocated on the heap, 
on the stack, or even within a [`VMValue`](../../thread/vmvalue.hpp) is managed by a corresponding `Block`.

*   **Core Attributes:** Each `Block` contains:
    *   **Type Information:** A reference to the [`Type`](../type_metadata/type.hpp) of the data it manages.
    *   **Data View:** A view (`base::ModRawView`) pointing to the actual memory region.
    *   **Reference Counting:** A `refcount` that tracks how many `Pointer`s currently refer to this block. 
        When the count drops to zero, the block becomes eligible for deallocation.
    *   **Deallocation Flag:** A boolean `deallocated` flag. When a block is freed, this flag is set. Any
        subsequent attempt to access data through a pointer to this block will result in a runtime
        error (a `VMUseAfterFreeException`), preventing silent memory corruption.
    *   **Child blocks:** For correctly handling memory errors in more complex structures (like an array, 
        struct or a variant) each block is enriched with a list of its children blocks, which reference a 
        sub-region of the data view handled by their parent. More on the the nested block structure is written 
        below.

#### The Nested Block Structure
One of the key memory module features is the ability for blocks to have a parent-child relationship. This hierarchical
structure is what enables safe implementation of complex types like variants, arrays and structs.

*   **How It Works:**

    A child `Block` does not allocate new memory. Instead, it acts as a **typed view** over a sub-region of its parent's
    memory. For example, a `Block` for a struct of type `VariantStruct` might manage 16 bytes. If its second field, `second`,
    is a variant and starts at offset 8, a child `Block` can be created that views the memory from offset 8 to 16 within the 
    parent block. This child block will have its own type (e.g., `SimpleVariant`).

*   **Why It's Crucial:**

    Imagine the following problem:
    ```cpp
    #include <variant>
    int main() {
        std::variant<int, bool> v;
        // Sets the type of variant to `int` and its value to 5.
        v = 5; 
        // Take the reference of the inner data of the variant.
        int& data_ref = std::get<int>(v); 
        // Sets the type of variant to `bool` and its value to false. 
        // `data_ref` should now be considered invalid.
        v = false; 
        data_ref = 3; // This is a UB
    }
    ``` 
    The above code is considered a UB in C++, but we want to detect this kind of memory errors in the VM deterministically.
    The nested block structure would enable us doing that as in the following way:
    1.  When you set the variant to hold an `int`, a child `Block` of type `int` would be created, viewing the parent's data. 
        Pointers to the inner value will point to this `int` child block (in this case `data_ref` would point to the child block of `v`).
    2.  If you later change the variant's active member to `bool`, the memory module performs a critical operation: 
        it **destroys the old `int` child block** (setting its `deallocated` flag) and creates a **new child `Block` of type `bool`** in its place.
    3.  Any old, dangling pointers that still refer to the `int` data now point to a deallocated block. If the program 
        attempts to use them, the memory module will detect this at access time and throw an exception, preventing
        a type confusion bug or use-after-free error.
        
We could imagine an even more complicated example where an array could store structs, which store variants and we take a pointer to one of the variant elements and then deallocate the outer array. With the nested block structure, any incorrect usages of this kind will be detected.

### [`Memory`](./memory.hpp)

The `Memory` class is the memory manager for a single `VMProcess`, orchestrating all the interactions between pointers and blocks. It is the single source of truth for the process's memory state.

*   **Lifecycle Management:** 
    The module handles the entire lifecycle of blocks. It creates them via allocators (`HeapAllocator`, 
    `DummyAllocator`), updates their reference counts as pointers are copied or destroyed, and ultimately frees their resources 
    when they are no longer needed.

*   **Data Integrity:** 
    When data is copied from one pointer to another (`vm::Memory::copyPointedData()`), the `Memory` module doesn't just perform an `std::memcpy`. It traverses the nested block hierarchy of the source, recreating the same structure at the destination which is crucial for detecting memory errors.

*   **Safety Enforcement and Validation:** 
    The centralized design of the `Memory` module is what enables the VM's powerful safety guarantees:
    *   **Use-After-Free Detection:** Every data access via `Memory::getPointerData` checks the block's `deallocated` flag.
    *   **Null Pointer Dereference:** Checks are performed to ensure the block reference is not `nullptr`.
    *   **Out-of-Bounds Access:** The offset and requested data size are checked against the block's view size.
    *   **Memory Leak Detection:** At the end of a program's execution, the `Memory` module can be audited to find any 
        blocks that were allocated but not freed, helping to identify memory leaks in the executed code.
        
<!-- @TODO: #1243 Write about the fast memory module once it exists-->