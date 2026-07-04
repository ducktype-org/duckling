# Test module for compiler-generated (and user-defined) copy constructors.

# Trivially copyable
class Trivial {
    a: i32;
    b: i32;
}

# Non-trivially-copyable because it owns a `box`
class HasBox {
    boxed: box i32;
}

# Non-trivially-copyable because it defines a user copy constructor.
class UserCopied {
    tag: i32 = 0;

    UserCopied.copy(other: const ref UserCopied) = {
        return UserCopied(other.tag);
    }
}

class HoldsNonTrivial {
    inner: HasBox;
}

class FinalBoss {
    # Trivially-copyable.
    i: i32;
    f: f64;
    flag: bool;

    # References & Pointers, trivially copyable.
    r: ref i32;
    p: ptr i32;
    c: cptr i32;
    m: manyptr i32;

    # Boxes, deep-copy.
    boxed_prim: box i32;        # Trivial pointee       -> box_of(*source)
    boxed_class: box HasBox;    # Non-trivial pointee   -> box_of(HasBox.__copy(...))

    # Static arrays
    trivial_arr: i32[4];        # Trivial element       -> whole array byte-copied
    nontrivial_arr: HasBox[3];  # Non-trivial element   -> array copy constructor

    # Tuples
    trivial_tup: (i32, f64);        # Trivial       -> byte-copied
    nontrivial_tup: (i32, HasBox);  # Non-trivial   -> tuple copy constructor

    # Lists, always deep-copied
    prim_list: List[i32];       # Trivial element     -> list copy constructor
    class_list: List[HasBox];   # Non-trivial element -> list copy constructor

    # Classes
    nested_default: HasBox;     # Generated copy constructor
    nested_user: UserCopied;    # User-defined copy constructor

    # Box of a user-copied class
    deep: box UserCopied;
}
