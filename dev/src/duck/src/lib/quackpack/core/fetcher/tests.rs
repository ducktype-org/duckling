use std::collections::HashMap;

use httpmock::prelude::*;
use tempfile::{TempDir, tempdir};

use super::*;
use crate::quackpack::core::Version;
use crate::quackpack::util::to_url::ToUrl;
use crate::util::test_utils::setup_test;

fn setup_duck_ctx() -> (DuckContext, TempDir) {
    let setup = || {
        // We set cache directory to a temporary directory, so we can use `Fetcher` without
        // worrying about leaving traces of tests in FS.
        let dir = tempdir().unwrap();
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::set_var("DUCK_HOME", dir.path());
        }
        let ctx = DuckContext::default();
        // SAFETY: Setup is single threaded, and `Env` in `DuckContext`, copies all envs.
        unsafe {
            std::env::remove_var("DUCK_HOME");
        }
        (ctx, dir)
    };
    setup_test(setup)
}

fn create_mock_server() -> MockServer {
    let pkg1 = registry::Dependency {
        name: "pkg1".into(),
        version: vec![Version::new(2, 3, 6)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: "https://google.com".into(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        alias: None,
    };

    let pkg2 = registry::Dependency {
        name: "pkg2".into(),
        version: vec![Version::new(2, 3, 4)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: "https://google.com".into(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        alias: None,
    };

    let pkg3 = registry::Dependency {
        name: "pkg3".into(),
        version: vec![Version::new(2, 4, 7)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: "https://google.com".into(),
            },
        },
        features: vec![],
        pinned: false,
        conditions: registry::DependencyCondition {
            package_features: None,
        },
        alias: None,
    };

    let bar_256 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 5, 6),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "bar".into(),
            description: "".into(),
        },
        dependencies: vec![pkg1],
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo_123 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 2, 3),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: vec![pkg2, pkg3],
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo_125 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 2, 5),
            authors: vec!["Carly Shillingford".into()],
            license: "MIT".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: registry::Dependencies::new(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let server = MockServer::start();

    // We explicitly disable those, so we can ensure, that we fetch them from a cache.
    // server.mock(|when, then| {
    //     when.method(GET).path("/packages/bar/2.5.6");
    //     then.status(200).json_body_obj(&bar_256);
    // });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo/1.2.5");
        then.status(404);
    });

    // server.mock(|when, then| {
    //     when.method(GET).path("/packages/foo/1.2.3");
    //     then.status(200).json_body_obj(&foo_123);
    // });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo/2137.6.7");
        then.status(404);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo");
        then.status(200).json_body_obj(&types::MultiMetadata {
            packages_metadata: vec![foo_123, foo_125],
        });
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/bar");
        then.status(200).json_body_obj(&types::MultiMetadata {
            packages_metadata: vec![bar_256],
        });
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/bar/2.5.6/download");
        then.status(200).body("bar-2.5.6");
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo/1.2.5/download");
        then.status(200).body("foo-1.2.5");
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo/1.2.3/download");
        then.status(200).body("foo-1.2.3");
    });

    server
}

#[test]
fn all_metadata_adds_to_cache() {
    let (ctx, _dir) = setup_duck_ctx();
    let server = create_mock_server();
    let mut fetcher = Fetcher::new(&ctx).unwrap();
    let response = fetcher
        .get_package_all_metadata(server.base_url().to_url().unwrap().into(), "foo".into())
        .unwrap();
    let FetcherResponse::Some(response) = response else {
        panic!("Offline response with offline flag not present");
    };
    assert_eq!(response.packages_metadata.len(), 2);
    let FetcherResponse::Some(fetched_from_cache) = fetcher
        .get_package_metadata(&types::PackageWithUrl {
            name: "foo".into(),
            version: Version::new(1, 2, 5),
            url: server.base_url().parse().unwrap(),
        })
        .unwrap()
    else {
        panic!("Offline response when metadata should be present in cache");
    };
    assert_eq!(fetched_from_cache.metadata.name, "foo");
    assert_eq!(fetched_from_cache.metadata.version, Version::new(1, 2, 5));
}

#[test]
fn without_cache_fetch_fails() {
    let (ctx, _dir) = setup_duck_ctx();
    let server = create_mock_server();
    let fetcher = Fetcher::new(&ctx).unwrap();
    let err = fetcher
        .get_package_metadata(&types::PackageWithUrl {
            name: "foo".into(),
            version: Version::new(1, 2, 5),
            url: server.base_url().parse().unwrap(),
        })
        .unwrap_err();
    assert_eq!(
        err.to_string(),
        format!(
            "while getting a metadata of `foo` version `1.2.5`
HTTP status client error (404) for url `{}/packages/foo/1.2.5`",
            server.base_url(),
        )
    );
}

fn create_sample_metadata() -> registry::Manifest {
    const JSON: &str = r#"{
    "metadata": {
        "authors": ["Me"],
        "version": "1.2.3",
        "name": "quackpack",
        "license": "GPS",
        "description": ""
    },
    "dependencies": [
        {
            "name": "pkg1",
            "version": ["2.3.4"],
            "source": {
                "inner": {
                    "type": "registry",
                    "registry-url": "xd"
                }
            },
            "features": [],
            "pinned": false,
            "conditions": {
                "package-features": []
            }
        }
    ],
    "dev-dependencies": [],
    "features": {},
    "profiles": {
      "dev": {
        "opt-level": "zero"
      },
      "foo": {
        "opt-level": "s"
      }
    }
}
    "#;
    serde_json::from_str(JSON).expect("statically known json")
}

#[test]
fn deserialize_tests() {
    let manifest = create_sample_metadata();
    let foo_profile = &manifest.profiles["foo"];
    assert_eq!(foo_profile.opt_level, Some(registry::OptLevel::S));

    let dev_profile = &manifest.profiles["dev"];
    assert_eq!(dev_profile.opt_level, Some(registry::OptLevel::Zero));
}
