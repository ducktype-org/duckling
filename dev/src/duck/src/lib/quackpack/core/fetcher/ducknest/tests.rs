use std::collections::HashMap;

use futures::executor::block_on;
use httpmock::prelude::*;
use tempfile::tempdir;

use super::*;
use crate::DuckContext;
use crate::quackpack::core::Version;
use crate::quackpack::core::fetcher::types;
use crate::quackpack::schemas::registry;
use crate::util::file_locks::FileLockManager;
use crate::util::path_ops_ext::PathOpsExt;

fn create_mock_server() -> (MockServer, DuckContext) {
    let pkg1 = registry::Dependency {
        name: "pkg1".into(),
        version: vec![Version::new(2, 3, 6)],
        source: registry::DependencySource {
            inner: registry::SourceInner::Registry {
                registry_url: "https://google.com".into(),
            },
        },
        kind: registry::DependencyKind::Normal,
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
        kind: registry::DependencyKind::Normal,
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
        kind: registry::DependencyKind::Normal,
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
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let server = MockServer::start();

    server.mock(|when, then| {
        when.method(GET).path("/packages/bar/2.5.6");
        then.status(200).json_body_obj(&bar_256);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo/1.2.5");
        then.status(200).json_body_obj(&foo_125);
    });

    server.mock(|when, then| {
        when.method(GET).path("/packages/foo/1.2.3");
        then.status(200).json_body_obj(&foo_123);
    });

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

    (server, DuckContext::default())
}

#[test]
fn single_metadata() {
    let (server, ctx) = create_mock_server();
    let http_client = Arc::new(AsyncHttpClient::new(&ctx));
    let client = DucknestClient::new(http_client);
    let response = block_on(client.get_exact_metadata(types::PackageWithUrl {
        name: "foo".into(),
        version: Version::new(1, 2, 3),
        url: server.base_url().parse().unwrap(),
    }))
    .unwrap();
    assert_eq!(response.metadata.name, "foo");
    assert_eq!(response.metadata.version, Version::new(1, 2, 3));
    assert_eq!(response.dependencies.len(), 2);

    assert!(
        block_on(client.get_exact_metadata(types::PackageWithUrl {
            name: "foo".into(),
            version: Version::new(1, 2, 4),
            url: server.base_url().parse().unwrap(),
        }))
        .is_err()
    );
}

#[test]
fn multi_metadata() {
    let (server, ctx) = create_mock_server();
    let http_client = Arc::new(AsyncHttpClient::new(&ctx));
    let client = DucknestClient::new(http_client);
    let response =
        block_on(client.get_multi_metadata(&(server.base_url().parse().unwrap()), "foo".into()))
            .unwrap();
    assert_eq!(response.packages_metadata.len(), 2);
}

#[test]
fn download_blob() {
    let dir = tempdir().unwrap();
    let (server, ctx) = create_mock_server();
    let manager = FileLockManager::new(dir.path().to_path_buf());
    let path = manager.open_exclusive("target", &ctx).unwrap();
    let http_client = Arc::new(AsyncHttpClient::new(&ctx));
    let client = DucknestClient::new(http_client);
    block_on(client.fetch_blob(
        types::PackageWithUrl {
            name: "foo".into(),
            version: Version::new(1, 2, 3),
            url: server.base_url().parse().unwrap(),
        },
        path,
    ))
    .unwrap();
    assert_eq!(
        dir.path().join("target").read_to_string().unwrap(),
        "foo-1.2.3"
    );
}

#[test]
fn not_found_in_response() {
    let (server, ctx) = create_mock_server();
    let http_client = Arc::new(AsyncHttpClient::new(&ctx));
    let client = DucknestClient::new(http_client);
    let err = block_on(client.get_exact_metadata(types::PackageWithUrl {
        name: "foo".into(),
        version: Version::new(2137, 6, 7),
        url: server.base_url().parse().unwrap(),
    }))
    .unwrap_err();
    assert_eq!(
        err.to_string(),
        format!(
            "HTTP status client error (404) for url `{}/packages/foo/2137.6.7`",
            server.base_url()
        )
    );
}
