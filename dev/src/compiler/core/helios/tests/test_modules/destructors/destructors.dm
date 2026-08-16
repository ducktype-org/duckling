# Trivially destructible -  all fields have no-op destructors.
class Trivial {
    a: i32;
    b: i32;
}

# Non-trivially-destructible because it owns a `box`.
class HasBox {
    boxed: box i32;
}

# Non-trivially-destructible because it declares a user destructor.
class UserDestroyed {
    tag: i32 = 0;

    UserDestroyed.destroy() = {
        var x: i32 = self.tag; # Just to check that it compiles.
    }
}

# Holds a non-trivial member, so its not a no-op.
class HoldsNonTrivial {
    inner: HasBox;
}

# A class that declares a user destructor and owns non-trivial members - the user code should run
# first, then the members are destroyed in reverse.
class UserAndMembers {
    first: HasBox;
    second: box i32;

    UserAndMembers.destroy() = {
        var y: i32 = 0;
    }
}

class FinalBoss {
    # Trivially-destructible.
    i: i32;
    flag: bool;

    # Boxes
    boxed_prim: box i32;                # Trivial pointee   -> just box_free
    boxed_class: box HasBox;            # Non-trivial pointee -> destroy pointee, then box_free

    # Static arrays
    trivial_arr: i32[4];                # Trivial element   -> no work
    nontrivial_arr: HasBox[3];          # Non-trivial element -> destroy loop

    # Tuples
    trivial_tup: (i32, bool);           # Trivial       -> no work
    nontrivial_tup: (i32, HasBox);      # Non-trivial   -> tuple destructor

    # Lists
    prim_list: List[i32];               # Trivial element   -> free buffer
    class_list: List[HasBox];           # Non-trivial element -> destroy loop

    # Classes
    nested_default: HasBox;             # Generated destructor
    nested_user: UserDestroyed;         # Runs the user destructor
}
