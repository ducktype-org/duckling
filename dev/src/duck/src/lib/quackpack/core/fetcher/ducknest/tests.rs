use std::collections::HashMap;

use crate::quackpack::core::fetcher::types;
use crate::util_common::path_ops_ext::PathOpsExt;

use super::*;
use crate::quackpack::core::Version;
use crate::quackpack::schemas::registry;
use tempfile::tempdir;
use wiremock::matchers::{method, path};
use wiremock::{Mock, MockServer, ResponseTemplate};

async fn create_mock_server() -> MockServer {
    let pkg1 = registry::Dependency {
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
        is_alias_for: None,
    };

    let pkg2 = registry::Dependency {
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
        is_alias_for: None,
    };

    let pkg3 = registry::Dependency {
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
        is_alias_for: None,
    };

    let bar_256 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(2, 5, 6),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "bar".into(),
            description: "".into(),
        },
        dependencies: [("pkg1".into(), pkg1)].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo_123 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 2, 3),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: [("pkg2".into(), pkg2), ("pkg3".into(), pkg3)].into(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let foo_125 = registry::Manifest {
        metadata: registry::Metadata {
            version: Version::new(1, 2, 5),
            authors: vec!["Patryk Rogalski".into()],
            license: "GLWTSPL".into(),
            name: "foo".into(),
            description: "".into(),
        },
        dependencies: registry::Dependencies::new(),
        dev_dependencies: registry::Dependencies::new(),
        features: HashMap::new(),
        profiles: HashMap::new(),
    };

    let server = MockServer::start().await;

    Mock::given(method("GET"))
        .and(path("/packages/bar/2.5.6"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&bar_256))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo/1.2.5"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&foo_125))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo/1.2.3"))
        .respond_with(ResponseTemplate::new(200).set_body_json(&foo_123))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo/2137.6.7"))
        .respond_with(ResponseTemplate::new(404))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo"))
        .respond_with(
            ResponseTemplate::new(200).set_body_json(&types::MultiMetadata {
                packages_metadata: vec![foo_123, foo_125],
            }),
        )
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/bar"))
        .respond_with(
            ResponseTemplate::new(200).set_body_json(&types::MultiMetadata {
                packages_metadata: vec![bar_256],
            }),
        )
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/bar/2.5.6/download"))
        .respond_with(ResponseTemplate::new(200).set_body_string("bar-2.5.6"))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo/1.2.3/download"))
        .respond_with(ResponseTemplate::new(200).set_body_string("foo-1.2.3"))
        .mount(&server)
        .await;

    Mock::given(method("GET"))
        .and(path("/packages/foo/1.2.5/download"))
        .respond_with(ResponseTemplate::new(200).set_body_string("foo-1.2.5"))
        .mount(&server)
        .await;

    server
}

#[tokio::test]
async fn single_metadata() {
    let server = create_mock_server().await;
    let client = DucknestClient::new().unwrap();
    let response = client
        .get_exact_metadata(&types::PackageWithUrl {
            id: "foo".into(),
            version: Version::new(1, 2, 3),
            url: server.uri().parse().unwrap(),
        })
        .await
        .unwrap();
    assert_eq!(response.metadata.name, "foo");
    assert_eq!(response.metadata.version, Version::new(1, 2, 3));
    assert_eq!(response.dependencies.len(), 2);

    assert!(
        client
            .get_exact_metadata(&types::PackageWithUrl {
                id: "foo".into(),
                version: Version::new(1, 2, 4),
                url: server.uri().parse().unwrap(),
            })
            .await
            .is_err()
    );
}

#[tokio::test]
async fn multi_metadata() {
    let server = create_mock_server().await;
    let client = DucknestClient::new().unwrap();
    let response = client
        .get_multi_metadata(&(server.uri().parse().unwrap()), "foo".into())
        .await
        .unwrap();
    assert_eq!(response.packages_metadata.len(), 2);
}

#[tokio::test]
async fn download_blob() {
    let dir = tempdir().unwrap();
    let path = dir.path().join("target");
    let server = create_mock_server().await;
    let client = DucknestClient::new().unwrap();
    client
        .fetch_blob(
            &types::PackageWithUrl {
                id: "foo".into(),
                version: Version::new(1, 2, 3),
                url: server.uri().parse().unwrap(),
            },
            &path,
        )
        .await
        .unwrap();
    assert_eq!(path.read_to_string().unwrap(), "foo-1.2.3");
}

#[tokio::test]
async fn not_found_in_response() {
    let server = create_mock_server().await;
    let client = DucknestClient::new().unwrap();
    let err = client
        .get_exact_metadata(&types::PackageWithUrl {
            id: "foo".into(),
            version: Version::new(2137, 6, 7),
            url: server.uri().parse().unwrap(),
        })
        .await
        .unwrap_err();
    assert_eq!(
        err.to_string(),
        format!(
            "while getting a metadata of `foo` version `2137.6.7` from `{}/`
HTTP status client error (404 Not Found) for url ({}/packages/foo/2137.6.7)",
            server.uri(),
            server.uri()
        )
    );
}
