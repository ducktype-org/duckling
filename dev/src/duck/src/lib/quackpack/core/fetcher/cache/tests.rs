use std::path::PathBuf;

use super::*;
use tempfile::{TempDir, tempdir};

use crate::{
    quackpack::core::{Version, fetcher::types::PackageWithUrl},
    quackpack::schemas::registry,
    static_str_id,
};

fn create_sample_metadata() -> registry::Manifest {
    const JSON: &str = r#"{
    "metadata": {
        "authors": ["Me"],
        "version": "1.2.3",
        "name": "quackpack",
        "license": "GPS",
        "description": ""
    },
    "dependencies": {
        "pkg1": {
            "version": ["2.3.4"],
            "source": {
                "inner": {
                    "type": "registry",
                    "registry_url": "xd"
                }
            },
            "features": [],
            "pinned": false,
            "conditions": {
                "package_features": []
            }
        }
    },
    "dev_dependencies": {},
    "features": {},
    "profiles": {}
}
    "#;
    serde_json::from_str(JSON).expect("statically known json")
}

fn create_example_package() -> PackageWithUrl {
    PackageWithUrl {
        id: static_str_id!("quackpack"),
        version: Version::new(1, 2, 3),
        url: Url::parse("https://localhost:9001").unwrap(),
    }
}

fn create_tmp_file() -> (TempDir, PathBuf) {
    let dir = tempdir().unwrap();
    let path = dir.path().join("test.db");
    (dir, path)
}

#[test]
fn can_create_metadata_cache_in_memory() {
    ManifestCache::new(CacheLocation::Memory).unwrap();
}

#[test]
fn can_create_metadata_cache_on_disk() {
    let (_dir, path) = create_tmp_file();
    ManifestCache::new(CacheLocation::Path(&path)).unwrap();
}

fn make_test_db() -> ManifestCache {
    ManifestCache::new(CacheLocation::Memory).unwrap()
}

#[tokio::test(flavor = "multi_thread")]
async fn add_and_fetch_metadata() {
    let cache = make_test_db();
    cache
        .add_or_replace_manifest(&create_example_package(), create_sample_metadata())
        .await
        .unwrap();
    let metadata = cache
        .get_manifest(&create_example_package())
        .await
        .unwrap()
        .unwrap();
    assert_eq!(metadata, create_sample_metadata());
}

#[tokio::test(flavor = "multi_thread")]
async fn not_found_metadata() {
    let cache = make_test_db();
    let metadata = cache.get_manifest(&create_example_package()).await.unwrap();
    assert_eq!(metadata, None);
}
