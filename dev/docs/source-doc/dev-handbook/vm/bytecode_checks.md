# Bytecode Checks

## Init and deinit

- Instructions operate on initialized memory
- Deinit is only called when stack is non-empty
- Local stack positions in instruction arguments point to the beginning of a variable

## Types

- Instructions operate on correct types
- Type names are unique

## Pointers

- Pointers have correct type assigned
- Operations on ArrPTR have correct offset (sizeof(T))
- Pointers are casted properly 

## Jumps

- Maintain stack structure between jumps
- Label names are unique

## Functions

- Function calls have correct types as arguments and return values
- Function names are unique
