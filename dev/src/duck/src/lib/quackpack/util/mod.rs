//! Various quackpack-only utilities.
pub mod interned_url;
pub mod paths;
pub mod progress_bar;
pub mod qp_context;
pub mod str_id;

/// A common message which should be passed to `.expect()`s.
pub const PANIC_MESSAGE: &str = "a thread panic'd, which should not have happened";
