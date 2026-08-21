#pragma once

// Every std adapter at once. They are separate headers because each one costs the
// standard header it wraps, and separate includes are the whole reason they are
// ser::serializer<T> specializations rather than rules inside dispatch: <ser/ser.hpp>
// serializes a type it has never heard of, and never pays for <map> to do it.
//
// Include this before the first ser::write or ser::read of a type that uses one of them.
// A specialization has to be declared before the use that would instantiate the primary
// template, which is the ordinary rule for any trait.

#include <ser/std/map.hpp>
#include <ser/std/optional.hpp>
#include <ser/std/string.hpp>
#include <ser/std/tuple.hpp>
#include <ser/std/vector.hpp>
