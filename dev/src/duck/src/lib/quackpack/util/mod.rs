//! Various quackpack-only utilities.
pub mod paths;
pub mod progress_bar;
pub mod qp_context;
pub mod str_id;

/// A common message which should be passed to `.expect()`s.
pub const PANIC_MESSAGE: &str = "a thread panick'ed, which should not have happened";
