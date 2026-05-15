//! Fetcher cache for a fetched manifest.
use std::path::Path;

use tracing::{debug, trace};
use url::Url;

use super::types;
use crate::quackpack::schemas::registry;

#[cfg(test)]
mod tests;

use crate::util::path_ops_ext::PathOpsExt;
use crate::{QuackResult, QuackResultContext};

const SQL_ERROR_MESSAGE: &str = "failed to execute an SQL query";

#[derive(Debug)]
/// Where should we cache metadata.
pub enum CacheLocation<'a> {
    /// Cache metadata in memory.
    Memory,
    /// Cache metadata in path.
    Path(&'a Path),
}

impl std::fmt::Display for CacheLocation<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match *self {
            CacheLocation::Memory => write!(f, "memory"),
            CacheLocation::Path(path) => write!(f, "file at `{}`", path.display()),
        }
    }
}

#[derive(Debug)]
/// All columns present in the database.
enum Columns {
    Name,
    Version,
    Url,
    ManifestJson,
}

impl Columns {
    /// Get the SQLite column name.
    const fn name(&self) -> &'static str {
        match self {
            Columns::Name => "name",
            Columns::Version => "version",
            Columns::Url => "url",
            Columns::ManifestJson => "manifest_json",
        }
    }

    /// Get the type of the SQLite column.
    const fn sqlite_type(&self) -> &'static str {
        match self {
            Columns::Name => "TEXT NOT NULL",
            Columns::Version => "TEXT NOT NULL",
            Columns::Url => "TEXT NOT NULL",
            Columns::ManifestJson => "TEXT NOT NULL",
        }
    }
}

/// Name of the table where we save metadata.
const TABLE_NAME: &str = "packages_manifests";

#[derive(Debug)]
/// Manager of a fetcher cache.
/// Database layout:
/// | package name NON NULL TEXT | package version NON NULL TEXT | registry URL NON NULL TEXT | package manifest json NON NULL TEXT |
/// |----------------------------|-------------------------------|----------------------------|-------------------------------------|
///
/// With the first three columns making a unique key.
///
/// Some notes about the layout:
/// 1. package version should follow [our semver](crate::quackpack::core::Version).
/// 2. URL should be a valid URL.
/// 3. Package manifest should be a JSON string which deserializes to the [Vec<registry::Manifest>](registry::Manifest),
///
/// However, SQLite doesn't expose a lot of types (unlike postgres, f.e.) so they are simply text.
pub struct ManifestCache {
    // According to the SQLite docs:
    // https://sqlite.org/c3ref/open.html
    //  SQLITE_OPEN_NOMUTEX
    //      The new database connection will use the "multi-thread" threading mode.
    //      This means that separate threads are allowed to use SQLite at the same time,
    //      as long as each thread is using a different database connection.
    //
    //  SQLITE_OPEN_FULLMUTEX
    //      The new database connection will use the "serialized" threading mode.
    //      This means the multiple threads can safely attempt to use the same database connection at the same time.
    //      (Mutexes will block any actual concurrency, but in this mode there is no harm in trying.)
    // And
    // https://sqlite.org/threadsafe.html
    //  Multi-thread.
    //      In this mode, SQLite can be safely used by multiple threads provided that
    //      no single database connection nor any object derived from database connection,
    //      such as a prepared statement, is used in two or more threads at the same time.
    //
    // But [rusqlite::Connection] is `!Sync` (even if it was opened with `SQLITE_OPEN_FULLMUTEX`), so on
    // rust side we would still have to *cheat* and manually implement `Sync`/wrap it with some type
    // which does that.
    //
    // This means, that we either:
    // 1. use defaults (`SQLITE_OPEN_NOMUTEX`) and mutable references, so borrow checker ensures SQLite safety,
    // 2. use defaults (`SQLITE_OPEN_NOMUTEX`) and immutable references + runtime borrow checking.
    //
    // Unfortunately, `Connection` always requires a mutable reference for creating transactions,
    // so there's no point in using `FULLMUTEX`, as we'll have to introduce some overhead on the rust side.
    connection: rusqlite::Connection,
}

impl ManifestCache {
    /// Create a new [ManifestCache] at location pointed by [CacheLocation].
    /// Note that this is a blocking operation.
    pub fn new(location: CacheLocation<'_>) -> QuackResult<Self> {
        debug!("initializing fetcher cache at `{location:?}`");
        let connection = match location {
            CacheLocation::Memory => rusqlite::Connection::open_in_memory()?,
            CacheLocation::Path(path) => {
                // Create a file before opening a connection, or playing with SQLite queries.
                // This should prevent hangs in CI/CD when creating a table: SQLite calls fsync, if
                // a file doesn't exist, at sometimes it can take a long time inside a docker.
                path.touch()?;
                rusqlite::Connection::open(path)?
            }
        };
        trace!("created connection");
        connection
            .create_table()
            .with_context(|| format!("failed to create a manifest cache table in {}", location))?;
        trace!("created table");
        Ok(Self { connection })
    }

    /// Get a cached manifest of package `package`.
    ///
    /// `Ok(Some)` means that manifest has been fetched successful, `Ok(None)`: we didn't have
    /// `package` in a cache, while `Err` indicates, most likely, internal SQL error.
    #[tracing::instrument(skip(self))]
    pub fn get_manifest(
        &self,
        package: &types::PackageWithUrl,
    ) -> QuackResult<Option<registry::Manifest>> {
        let maybe_json = self
            .connection
            .get_single_manifest_json(package)
            .context(SQL_ERROR_MESSAGE)?;
        maybe_json
            .map(|json| {
                serde_json::from_str(&json)
                    .context_internal("failed to deserialize previously serialized manifest json")
            })
            .transpose()
    }

    /// Get all cached manifests of package `package`.
    ///
    /// `Ok` means that manifest has been fetched successful, while `Err` indicates, most likely, internal SQL error.
    ///
    /// Note that `Ok` allows inner `packages_manifest` to be an empty Vec: it means that we don't
    /// have `package` in a cache.
    #[tracing::instrument(skip(self))]
    pub fn get_all_manifests(
        &self,
        package: &types::PackageWithUrl,
    ) -> QuackResult<Vec<registry::Manifest>> {
        let jsons = self
            .connection
            .get_all_manifests_json(package)
            .context(SQL_ERROR_MESSAGE)?;
        jsons
            .into_iter()
            .map(|json| serde_json::from_str(&json))
            .collect::<Result<_, _>>()
            .context_internal("failed to deserialize previously serialized manifest json")
    }

    /// Add or replace manifest for package `package`.
    ///
    /// `Ok` means that manifest has been added successful, while `Err` indicates, most likely, internal SQL error.
    #[tracing::instrument(skip(self))]
    pub fn add_or_replace_manifest(
        &self,
        package: &types::PackageWithUrl,
        manifest: registry::Manifest,
    ) -> QuackResult<()> {
        let json = serde_json::to_string(&manifest)
            .context_internal("failed to serialize registry schema to JSON")?;
        self.connection
            .add_or_replace_manifest_json(package, json)
            .context(SQL_ERROR_MESSAGE)?;
        Ok(())
    }

    /// Add or replace multiple manifest for package `package`.
    ///
    /// This function can be more efficient than calling
    /// [`add_or_replace_multi_manifest`](Self::add_or_replace_multi_manifest) multiple times, due
    /// to inner SQL locking.
    ///
    /// `Ok` means that manifest has been added successful, while `Err` indicates, most likely, internal SQL error.
    #[tracing::instrument(skip(self))]
    pub fn add_or_replace_multiple_manifests(
        &mut self,
        registry_url: Url,
        multi_manifest: Vec<registry::Manifest>,
    ) -> QuackResult<()> {
        let package_manifest_pairs = multi_manifest
            .into_iter()
            .map(|manifest| {
                let json = serde_json::to_string(&manifest)
                    .context_internal("failed to serialize registry schema to JSON")?;
                let package = types::PackageWithUrl {
                    id: manifest.metadata.name.into(),
                    version: manifest.metadata.version,
                    url: registry_url.clone(),
                };
                Ok((package, json))
            })
            .collect::<QuackResult<_>>()?;
        self.connection
            .add_or_replace_mutliple_manifests_jsons(package_manifest_pairs)
            .context(SQL_ERROR_MESSAGE)?;
        Ok(())
    }
}

trait ConnectionExt {
    /// Execute given `sql` query with `params`, ensuring that exactly zero or one rows have been
    /// returend.
    /// If and only if exactly one row has been returned, call `f` on it, returning
    /// `Ok(Some(f(row))`.
    /// If there are no rows, returns `Ok(None)`, and `Err` if there was more
    /// than one row or inner SQL query had errored.
    fn query_maybe_one<T, P, F>(&self, sql: &str, params: P, f: F) -> rusqlite::Result<Option<T>>
    where
        P: rusqlite::Params,
        F: FnOnce(&rusqlite::Row<'_>) -> rusqlite::Result<T>;

    /// Helper for extracting JSON manifest of a `package`.
    fn get_single_manifest_json(
        &self,
        package: &types::PackageWithUrl,
    ) -> rusqlite::Result<Option<String>>;

    /// Helper for extracting all JSON manifests of a `package`.
    fn get_all_manifests_json(
        &self,
        package: &types::PackageWithUrl,
    ) -> rusqlite::Result<Vec<String>>;

    /// Helper for creating internal cache table.
    fn create_table(&self) -> rusqlite::Result<()>;

    /// Helper for adding or replacing JSON manifest of a `package`.
    fn add_or_replace_manifest_json(
        &self,
        package: &types::PackageWithUrl,
        manifest_json: String,
    ) -> rusqlite::Result<()>;

    /// Helper for adding or replacing multiple JSON manifests of packages.
    fn add_or_replace_mutliple_manifests_jsons(
        &mut self,
        multi_manifests: Vec<(types::PackageWithUrl, String)>,
    ) -> rusqlite::Result<()>;
}

impl ConnectionExt for rusqlite::Connection {
    fn query_maybe_one<T, P, F>(&self, sql: &str, params: P, f: F) -> rusqlite::Result<Option<T>>
    where
        P: rusqlite::Params,
        F: FnOnce(&rusqlite::Row<'_>) -> rusqlite::Result<T>,
    {
        use rusqlite::OptionalExtension;
        self.query_one(sql, params, f).optional()
    }

    fn get_single_manifest_json(
        &self,
        package: &types::PackageWithUrl,
    ) -> rusqlite::Result<Option<String>> {
        self.query_maybe_one(
            &format!(
                "SELECT {manifest} FROM {table} WHERE {name} = :package_name AND {version} = :package_version AND {url} = :package_url",
                manifest = Columns::ManifestJson.name(),
                table = TABLE_NAME,
                name = Columns::Name.name(),
                version = Columns::Version.name(),
                url = Columns::Url.name(),
            ),
            rusqlite::named_params![
                ":package_name": package.id.as_str(),
                ":package_version": package.version.to_string(),
                ":package_url": package.url.as_str(),
            ],
            |row| row.get(0),
        )
    }

    fn create_table(&self) -> rusqlite::Result<()> {
        self.execute(
            &format!(
                "CREATE TABLE IF NOT EXISTS {table} (
                    {name} {name_type},
                    {version} {version_type},
                    {url} {url_type},
                    {manifest} {manifest_type},
                    PRIMARY KEY ({name}, {version}, {url})
                );",
                table = TABLE_NAME,
                name = Columns::Name.name(),
                name_type = Columns::Name.sqlite_type(),
                version = Columns::Version.name(),
                version_type = Columns::Version.sqlite_type(),
                url = Columns::Url.name(),
                url_type = Columns::Url.sqlite_type(),
                manifest = Columns::ManifestJson.name(),
                manifest_type = Columns::ManifestJson.sqlite_type(),
            ),
            [],
        )?;
        Ok(())
    }

    fn get_all_manifests_json(
        &self,
        package: &types::PackageWithUrl,
    ) -> rusqlite::Result<Vec<String>> {
        let mut stmt = self.prepare(&format!(
            "SELECT {manifest} FROM {table} WHERE {name} = :package_name AND {url} = :package_url;",
            manifest = Columns::ManifestJson.name(),
            table = TABLE_NAME,
            name = Columns::Name.name(),
            url = Columns::Url.name(),
        ))?;
        stmt.query_map(
            rusqlite::named_params![
                ":package_name": package.id.as_str(),
                ":package_url": package.url.as_str(),
            ],
            |row| row.get(0),
        )?
        .collect()
    }

    fn add_or_replace_manifest_json(
        &self,
        package: &types::PackageWithUrl,
        manifest_json: String,
    ) -> rusqlite::Result<()> {
        self.execute(
            &format!(
                "INSERT OR REPLACE INTO {table} ({name}, {version}, {url}, {manifest_json_column})
                 VALUES (:package_name, :package_version, :package_url, :package_manifest);",
                table = TABLE_NAME,
                name = Columns::Name.name(),
                version = Columns::Version.name(),
                url = Columns::Url.name(),
                manifest_json_column = Columns::ManifestJson.name(),
            ),
            rusqlite::named_params![
                ":package_name": package.id.as_str(),
                ":package_version": package.version.to_string(),
                ":package_url": package.url.as_str(),
                ":package_manifest": manifest_json,
            ],
        )?;
        Ok(())
    }

    fn add_or_replace_mutliple_manifests_jsons(
        &mut self,
        multi_manifests: Vec<(types::PackageWithUrl, String)>,
    ) -> rusqlite::Result<()> {
        let tx = self.transaction()?;
        let mut stmt = tx.prepare(&format!(
            "INSERT OR REPLACE INTO {table} ({name}, {version}, {url}, {manifest})
             VALUES (:package_name, :package_version, :package_url, :package_manifest);",
            table = TABLE_NAME,
            name = Columns::Name.name(),
            version = Columns::Version.name(),
            url = Columns::Url.name(),
            manifest = Columns::ManifestJson.name(),
        ))?;
        for (package, manifest) in multi_manifests {
            stmt.execute(rusqlite::named_params![
                ":package_name": package.id.as_str(),
                ":package_version": package.version.to_string(),
                ":package_url": package.url.as_str(),
                ":package_manifest": manifest,
            ])?;
        }
        drop(stmt);
        tx.commit()
    }
}
