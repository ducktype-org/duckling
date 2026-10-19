// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

//! Various SQLite3 helpers.

use std::collections::HashSet;

use rusqlite::Connection;
use tracing::trace;

#[derive(Debug)]
/// A generic SQLite3 migration.
pub struct Migration {
    /// Name of the migration. It's _not_ the primary key, _nor_ part of it.
    pub name: &'static str,
    /// Version of the migration. _This_ is the primary key.
    pub version: i32,
    /// SQL which will be executed as part of the migration.
    pub sql: &'static str,
}

/// Run needed migrations.
///
/// Migrations are tracked in a separate table `schema_migrations`. In addition to the unique key
/// (being the [`Migration::version`]), it also tracks the name and timestamp of the migration.
pub fn run_migrations(
    connection: &mut Connection,
    migrations: &[Migration],
) -> rusqlite::Result<()> {
    debug_assert!(migrations.is_sorted_by_key(|migration| migration.version));
    if cfg!(debug_assertions) {
        let number_of_migrations = migrations.len();
        let versions = migrations
            .iter()
            .map(|migration| migration.version as usize);
        let expected_versions = 1..=number_of_migrations;
        assert!(versions.eq(expected_versions));
    }
    let tx = connection.transaction_with_behavior(rusqlite::TransactionBehavior::Exclusive)?;
    tx.execute(
        "CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            applied_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );",
        [],
    )?;

    let versions_of_applied_migrations = {
        let mut stmt = tx.prepare("SELECT version FROM schema_migrations")?;
        stmt.query_map([], |row| row.get(0))?
            .collect::<Result<HashSet<i32>, _>>()?
    };

    let mut applied_count = 0;

    for migration in migrations {
        if !versions_of_applied_migrations.contains(&migration.version) {
            tx.execute_batch(migration.sql)?;

            tx.execute(
                "INSERT INTO schema_migrations (version, name) VALUES (?1, ?2);",
                rusqlite::params![migration.version, migration.name],
            )?;

            trace!(migration.version = %migration.version, migration.name = %migration.name, "executed migration");
            applied_count += 1;
        }
    }

    tx.commit()?;

    trace!(%applied_count, "finished migrations");

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_successful_sequential_migrations() {
        let mut conn = Connection::open_in_memory().unwrap();

        let migrations = [
            Migration {
                version: 1,
                name: "create_users",
                sql: "CREATE TABLE users (id INTEGER PRIMARY KEY);",
            },
            Migration {
                version: 2,
                name: "create_posts",
                sql: "CREATE TABLE posts (id INTEGER PRIMARY KEY);",
            },
        ];

        run_migrations(&mut conn, &migrations).unwrap();

        let count: i32 = conn
            .query_row("SELECT COUNT(*) FROM schema_migrations", [], |row| {
                row.get(0)
            })
            .unwrap();
        assert_eq!(count, 2);

        let users_exists: bool = conn
            .query_row(
                "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='users'",
                [],
                |row| Ok(row.get::<_, i32>(0)? > 0),
            )
            .unwrap();
        assert!(users_exists);
    }

    #[test]
    fn test_idempotency_running_twice() {
        let mut conn = Connection::open_in_memory().unwrap();

        let migrations = [Migration {
            version: 1,
            name: "create_users",
            sql: "CREATE TABLE users (id INTEGER PRIMARY KEY);",
        }];

        run_migrations(&mut conn, &migrations).unwrap();

        // Second migrations do nothing.
        run_migrations(&mut conn, &migrations).unwrap();

        let count: i32 = conn
            .query_row("SELECT COUNT(*) FROM schema_migrations", [], |row| {
                row.get(0)
            })
            .unwrap();
        assert_eq!(count, 1);
    }
}
