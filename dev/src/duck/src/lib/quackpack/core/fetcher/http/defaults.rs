//! Default values for [`HttpClient`](super::HttpClient).
use std::time::Duration;

// https://docs.rs/reqwest/latest/reqwest/struct.ClientBuilder.html#method.user_agent
/// Duck's user agent value.
pub const DUCK_USER_AGENT: &str = concat!(env!("CARGO_PKG_NAME"), "/", env!("CARGO_PKG_VERSION"),);

/// NOTE: From [docs](https://docs.rs/curl/latest/curl/easy/struct.Easy2.html#method.http_headers):
/// headers without value follow `MyHeader;` syntax.
///
/// NOTE: Comments below are copied from `curl_http_client.py`.
///
/// libcurl's magic "Expect: 100-continue" behavior causes delays
/// with servers that don't support it (which include, among others,
/// Google's OpenID endpoint).  Additionally, this behavior has
/// a bug in conjunction with the curl_multi_socket_action API
/// (https://sourceforge.net/tracker/?func=detail&atid=100976&aid=3039744&group_id=976),
/// which increases the delays. It's more trouble than it's worth,
/// so just turn off the feature (yes, setting Expect: to an empty
/// value is the official way to disable this)
pub const EXPECT_HEADER_WITH_VALUE: &str = "Expect;";
/// NOTE: Comments below are copied from `curl_http_client.py`.
///
/// libcurl adds Pragma: no-cache by default; disable that too
pub const PRAGMA_HEADER_WITH_VALUE: &str = "Pragma;";
/// An arbitrary value.
pub const CONNECT_TIMEOUT: Duration = Duration::from_secs(60);
/// An arbitrary value.
pub const REQUEST_TIMEOUT: Duration = Duration::from_secs(60);

/// Maximal allowed number of redirects.
pub const MAX_REDIRECTS: usize = 5;
/// `application/json` value for the Content-type header.
pub const _APPLICATION_JSON: &str = "application/json";
