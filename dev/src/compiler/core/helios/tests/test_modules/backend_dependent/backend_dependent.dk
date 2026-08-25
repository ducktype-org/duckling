namespace getValue {
    @backend_dependent
    fundecl func() -> i32;

    @dvm_only_impl
    fun func() = {
        return 10;
    }

    @native_only_impl
    fun func() = {
        return 20;
    }
}

# Comp time evaluation runs on the DVM, so this should select the
# `@dvm_only_impl` implementation and evaluate to 10.
const VALUE = getValue.func();
