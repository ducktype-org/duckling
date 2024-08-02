/**
 * @file smart_pointers.hpp
 *
 *
 * Smart pointers are useful for resource management. Rather than use `std::unique_ptr`
 * we use our extended version in `base::unique_ptr` that adds the ability to create
 * a `base::borrow_ptr` from it that can access (and possibly modify) the underlying value.
 * If the original `base::unique_ptr` is deleted then all `base::borrow_ptr` that were
 * borrowing from it are invalid and using them will lead to undefined behavior.
 *
 * ### Usage:
 * @include smart_pointers_example.cpp
 *
 * @example smart_pointers_example.cpp
 */
#pragma once

#include "borrow_pointer.hpp"
#include "unique_pointer.hpp"
