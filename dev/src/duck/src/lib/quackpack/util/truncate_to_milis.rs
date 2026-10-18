// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

use chrono::{DateTime, Utc};

/// Truncate [`DateTime<Utc>`] down to miliseconds.
pub fn truncate_to_millis(dt: DateTime<Utc>) -> DateTime<Utc> {
    // Get the nanoseconds part of the current second (0..1_999_999_999, leap seconds included)
    let nanos = dt.timestamp_subsec_nanos();
    // Truncate to millisecond precision
    let truncated_nanos = (nanos / 1_000_000) * 1_000_000;
    // Reconstruct DateTime using the original seconds timestamp and truncated nanoseconds
    DateTime::from_timestamp(dt.timestamp(), truncated_nanos)
        .expect("truncated nanos should be in valid range")
}
