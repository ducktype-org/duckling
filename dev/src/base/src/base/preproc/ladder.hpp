#pragma once

#include <base/preproc/cat.hpp>

#include <cstddef>

/**
 * @file
 * @brief The structured-bindings arity table: one rung per member count, 1 to
 * `base::LADDER_MAX`.
 * @details A binding declaration spells its arity out - `auto&& [a, b] = obj;` - so "hand me
 * every member" needs one branch per possible member count. `BASE_LADDER(RUNG)` expands
 * `RUNG(n)` for every count, and `BASE_LADDER_NAMES(n)` hands a rung its n names. Each entry of
 * the name table is the previous one plus a name, so a typo is either a duplicate binding or a
 * missing one, and both are compile errors.
 *
 * The arity of a structured binding is CHECKED against the type and never deduced, so a rung
 * that does not match the type fails to compile rather than quietly binding the wrong count.
 *
 * @note `base/preproc/while.hpp`'s `REPEAT` cannot generate this: it counts with `DEC` from
 * macro_base.hpp, which is defined only for single digits. Do not "simplify" the table into it
 * without extending that arithmetic to 64 first.
 * @note This header is the table only. The rungs that need types - walking members, collecting
 * their declared types - are in base/comptime/member_walk.hpp.
 */

namespace base {

	/** @brief The largest member count the table below covers. */
	inline constexpr ::std::size_t LADDER_MAX = 64;

}  // namespace base

/** @brief The binding names of rung `n`: m0, m1, ... m(n-1). */
#define BASE_LADDER_NAMES(n) CAT(BASE_LADDER_NAMES_, n)

// clang-format off
#define BASE_LADDER_NAMES_1  m0
#define BASE_LADDER_NAMES_2  BASE_LADDER_NAMES_1, m1
#define BASE_LADDER_NAMES_3  BASE_LADDER_NAMES_2, m2
#define BASE_LADDER_NAMES_4  BASE_LADDER_NAMES_3, m3
#define BASE_LADDER_NAMES_5  BASE_LADDER_NAMES_4, m4
#define BASE_LADDER_NAMES_6  BASE_LADDER_NAMES_5, m5
#define BASE_LADDER_NAMES_7  BASE_LADDER_NAMES_6, m6
#define BASE_LADDER_NAMES_8  BASE_LADDER_NAMES_7, m7
#define BASE_LADDER_NAMES_9  BASE_LADDER_NAMES_8, m8
#define BASE_LADDER_NAMES_10 BASE_LADDER_NAMES_9, m9
#define BASE_LADDER_NAMES_11 BASE_LADDER_NAMES_10, m10
#define BASE_LADDER_NAMES_12 BASE_LADDER_NAMES_11, m11
#define BASE_LADDER_NAMES_13 BASE_LADDER_NAMES_12, m12
#define BASE_LADDER_NAMES_14 BASE_LADDER_NAMES_13, m13
#define BASE_LADDER_NAMES_15 BASE_LADDER_NAMES_14, m14
#define BASE_LADDER_NAMES_16 BASE_LADDER_NAMES_15, m15
#define BASE_LADDER_NAMES_17 BASE_LADDER_NAMES_16, m16
#define BASE_LADDER_NAMES_18 BASE_LADDER_NAMES_17, m17
#define BASE_LADDER_NAMES_19 BASE_LADDER_NAMES_18, m18
#define BASE_LADDER_NAMES_20 BASE_LADDER_NAMES_19, m19
#define BASE_LADDER_NAMES_21 BASE_LADDER_NAMES_20, m20
#define BASE_LADDER_NAMES_22 BASE_LADDER_NAMES_21, m21
#define BASE_LADDER_NAMES_23 BASE_LADDER_NAMES_22, m22
#define BASE_LADDER_NAMES_24 BASE_LADDER_NAMES_23, m23
#define BASE_LADDER_NAMES_25 BASE_LADDER_NAMES_24, m24
#define BASE_LADDER_NAMES_26 BASE_LADDER_NAMES_25, m25
#define BASE_LADDER_NAMES_27 BASE_LADDER_NAMES_26, m26
#define BASE_LADDER_NAMES_28 BASE_LADDER_NAMES_27, m27
#define BASE_LADDER_NAMES_29 BASE_LADDER_NAMES_28, m28
#define BASE_LADDER_NAMES_30 BASE_LADDER_NAMES_29, m29
#define BASE_LADDER_NAMES_31 BASE_LADDER_NAMES_30, m30
#define BASE_LADDER_NAMES_32 BASE_LADDER_NAMES_31, m31
#define BASE_LADDER_NAMES_33 BASE_LADDER_NAMES_32, m32
#define BASE_LADDER_NAMES_34 BASE_LADDER_NAMES_33, m33
#define BASE_LADDER_NAMES_35 BASE_LADDER_NAMES_34, m34
#define BASE_LADDER_NAMES_36 BASE_LADDER_NAMES_35, m35
#define BASE_LADDER_NAMES_37 BASE_LADDER_NAMES_36, m36
#define BASE_LADDER_NAMES_38 BASE_LADDER_NAMES_37, m37
#define BASE_LADDER_NAMES_39 BASE_LADDER_NAMES_38, m38
#define BASE_LADDER_NAMES_40 BASE_LADDER_NAMES_39, m39
#define BASE_LADDER_NAMES_41 BASE_LADDER_NAMES_40, m40
#define BASE_LADDER_NAMES_42 BASE_LADDER_NAMES_41, m41
#define BASE_LADDER_NAMES_43 BASE_LADDER_NAMES_42, m42
#define BASE_LADDER_NAMES_44 BASE_LADDER_NAMES_43, m43
#define BASE_LADDER_NAMES_45 BASE_LADDER_NAMES_44, m44
#define BASE_LADDER_NAMES_46 BASE_LADDER_NAMES_45, m45
#define BASE_LADDER_NAMES_47 BASE_LADDER_NAMES_46, m46
#define BASE_LADDER_NAMES_48 BASE_LADDER_NAMES_47, m47
#define BASE_LADDER_NAMES_49 BASE_LADDER_NAMES_48, m48
#define BASE_LADDER_NAMES_50 BASE_LADDER_NAMES_49, m49
#define BASE_LADDER_NAMES_51 BASE_LADDER_NAMES_50, m50
#define BASE_LADDER_NAMES_52 BASE_LADDER_NAMES_51, m51
#define BASE_LADDER_NAMES_53 BASE_LADDER_NAMES_52, m52
#define BASE_LADDER_NAMES_54 BASE_LADDER_NAMES_53, m53
#define BASE_LADDER_NAMES_55 BASE_LADDER_NAMES_54, m54
#define BASE_LADDER_NAMES_56 BASE_LADDER_NAMES_55, m55
#define BASE_LADDER_NAMES_57 BASE_LADDER_NAMES_56, m56
#define BASE_LADDER_NAMES_58 BASE_LADDER_NAMES_57, m57
#define BASE_LADDER_NAMES_59 BASE_LADDER_NAMES_58, m58
#define BASE_LADDER_NAMES_60 BASE_LADDER_NAMES_59, m59
#define BASE_LADDER_NAMES_61 BASE_LADDER_NAMES_60, m60
#define BASE_LADDER_NAMES_62 BASE_LADDER_NAMES_61, m61
#define BASE_LADDER_NAMES_63 BASE_LADDER_NAMES_62, m62
#define BASE_LADDER_NAMES_64 BASE_LADDER_NAMES_63, m63

/** @brief Expands `RUNG(n)` for every member count from 1 to base::LADDER_MAX. */
#define BASE_LADDER(RUNG) \
	RUNG(1) RUNG(2) RUNG(3) RUNG(4) RUNG(5) RUNG(6) RUNG(7) RUNG(8) \
	RUNG(9) RUNG(10) RUNG(11) RUNG(12) RUNG(13) RUNG(14) RUNG(15) RUNG(16) \
	RUNG(17) RUNG(18) RUNG(19) RUNG(20) RUNG(21) RUNG(22) RUNG(23) RUNG(24) \
	RUNG(25) RUNG(26) RUNG(27) RUNG(28) RUNG(29) RUNG(30) RUNG(31) RUNG(32) \
	RUNG(33) RUNG(34) RUNG(35) RUNG(36) RUNG(37) RUNG(38) RUNG(39) RUNG(40) \
	RUNG(41) RUNG(42) RUNG(43) RUNG(44) RUNG(45) RUNG(46) RUNG(47) RUNG(48) \
	RUNG(49) RUNG(50) RUNG(51) RUNG(52) RUNG(53) RUNG(54) RUNG(55) RUNG(56) \
	RUNG(57) RUNG(58) RUNG(59) RUNG(60) RUNG(61) RUNG(62) RUNG(63) RUNG(64)
// clang-format on
