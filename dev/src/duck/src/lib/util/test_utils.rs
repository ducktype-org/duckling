use std::sync::{LazyLock, Mutex};

static TESTS_SETUP_LOCK: LazyLock<Mutex<()>> = LazyLock::new(Mutex::default);

/// A wrapper for setting up tests which modify shared resources (e.g. environmental variables).
pub fn setup_test<F, R>(setup: F) -> R
where
    F: FnOnce() -> R,
{
    // Since a result of a test should be independent of the results of other tests,
    // we should not care about mutex poisoning.
    let _guard = TESTS_SETUP_LOCK.lock();
    setup()
}
