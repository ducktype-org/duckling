# This test aims to verify correct handling of:
# - Unit and tuple values and their types
# - Unit and tuple values being lifted to types when necessary
# - Single values and types in parentheses being treated as single values/types, not tuples

#############
#   UNITS   #
#############

const Unit1          = ();  # Expect deduction of unit type from unit value.
const Unit2: ()      = ();  # Expect lifting a unit value to a unit type in type annotation.
const UnitType: type = ();  # Expect lifting a unit value to a unit type in value definition.

#####################
#   SINGLE VALUES   #
#####################

const Int1          = (1);    # Expect deduction of i32 type from i32 value.
const Int2: (i32)   = (2);    # Expect using a parenthesised type annotation as type.
const IntType: type = (i32);  # Expect using a parenthesised value definition as type.

const IntType1          = (i32);   # Expect deduction of meta type from type value.
const IntType2: (type)  = (i32);   # Expect using a parenthesised type annotation as type.
const IntTypeType: type = (type);  # Expect using a parenthesised value definition as type.

##############
#   TUPLES   #
##############

const TupleII1             = (1, 2);      # Expect deduction.
const TupleII2: (i32, i32) = (3, 4);      # Expect lifting in type annotation.
const TupleIIType: type    = (i32, i32);  # Expect lifting in value definition.

const TupleTT1               = (i32, bool);   # Expect deduction.
const TupleTT2: (type, type) = (i32, bool);   # Expect lifting in type annotation.
const TupleTTType: type      = (type, type);  # Expect lifting in value definition.

const TupleLiftError: type = (i32, 1);  # Expect error.
