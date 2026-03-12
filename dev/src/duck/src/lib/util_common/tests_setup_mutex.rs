use std::sync::{LazyLock, Mutex};

static TESTS_SETUP_LOCK: LazyLock<Mutex<()>> = LazyLock::new(Mutex::default);

pub fn setup_test<F, R>(setup: F) -> R
where
    F: FnOnce() -> R,
{
    let _guard = TESTS_SETUP_LOCK.lock();
    setup()
}
