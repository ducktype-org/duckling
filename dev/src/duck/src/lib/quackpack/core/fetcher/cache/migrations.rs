// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! SQLite3 migrations used by Ducknest's SQLite3 cache.

use crate::quackpack::util::sqlite::Migration;

// Important: make sure types and names match constants/enums from the main file.
const CREATE_BASIC_TABLE: Migration = Migration {
    name: "create_basic_table",
    version: 1,
    sql: "CREATE TABLE packages_manifests (
            name TEXT NOT NULL,
            version TEXT NOT NULL,
            url TEXT NOT NULL,
            manifest_json TEXT NOT NULL,
            PRIMARY KEY (name, version, url)
         );",
};

pub(super) const MIGRATIONS: &[Migration] = &[CREATE_BASIC_TABLE];

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn migrations_are_sorted_by_version() {
        for (idx, migration) in MIGRATIONS.iter().enumerate() {
            assert_eq!(
                i32::try_from(idx + 1).unwrap(),
                migration.version,
                "{}th migration should have version {}",
                idx + 1,
                idx + 1
            );
        }
    }
}
