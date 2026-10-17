#pragma once

/**
 * @file
 * @brief Every base adapter at once, and the same trade as <ser/std/all.hpp>: separate headers,
 * @details because each one costs the base header it wraps.
 *
 * Include this before the first ser::write or ser::read of a type that uses one of them - a
 * specialization has to be declared before the use that would instantiate the primary
 * template, which is the ordinary rule for any trait.
 *
 * Several of these adapters exist to REFUSE a type - Ref, SharedBox, BoxOrCRef, RawView,
 * ModRawView, CheckedOkBad. That is not a gap waiting to be filled: each one names what to
 * write instead, and the refusal is only visible if the header is included, so leaving it
 * out is how a struct with a Ref field ends up reported as "looks pointer-like".
 *
 * What is deliberately NOT here, and why:
 *
 *   base::StableObjectPool     slot recycling means the free list is part of the state, and
 *                              whether a recycled slot is the same object as the one it
 *                              replaced is the question a stream cannot answer.
 *   MAKE_FLAG_TYPE flag types  the bitmask is private and the only public way in is
 *                              operator|= with an enumerator, whose type the flag class
 *                              does not name - so an adapter cannot rebuild an arbitrary
 *                              mask. Give the flag type a serVisit, or serialize the mask
 *                              through your own accessor.
 *   base::ManualLifetimeStorage  raw storage: nothing in it says whether the object is
 *                              alive, which is the one thing a read would have to know.
 */

#include <ser/base/bitset.hpp>
#include <ser/base/maps.hpp>
#include <ser/base/optional.hpp>
#include <ser/base/pointers.hpp>
#include <ser/base/refs.hpp>
#include <ser/base/types.hpp>
#include <ser/base/vectors.hpp>
#include <ser/base/views.hpp>
