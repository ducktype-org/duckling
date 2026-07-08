//! Various quackpack-only utilities.
pub mod http;
pub mod interned_url;
pub mod is_local_file;
pub mod paths;
pub mod progress_bar;
pub mod qp_context;
pub mod str_id;
pub mod to_path_buf;
pub mod to_url;
pub mod with_version;

/// A common message which should be passed to `.expect()`s.
pub const PANIC_MESSAGE: &str = "a thread panic'd, which should not have happened";
