//! Schema of the multipackage JSON sent to duckc from QuackPack
//!
//! There are a few nuances we have to remember about:
//! * when compiling a package, we need to pass its __entire__ subtree to duckc,
//! * only ids specified in [`tasks`](DuckcMultiPackage::tasks) will be compiled; this effectively
//!   allows us to compile a single package, entire graph, or a chosen subset.

use std::path::PathBuf;

use serde::{Deserialize, Serialize, de, ser};

use crate::StrId;
use crate::quackpack::core::{FeatureName, Version};

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct MultiPackage {
    /// Packages required for a successful duckc invocation; packages have to contain their __entire__ dependencies trees.
    pub packages: Vec<Package>,
    /// Duckc tasks to finish in this invocation.
    pub tasks: Vec<Task>,
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct Package {
    #[serde(rename = "name")]
    /// ID of a package to compile. Note, that it doesn't have to be a package's name, it can be a
    /// unique id.
    /// Has to be unique in terms of the entire [`packages`](DuckcMultiPackage::packages) vector.
    pub id: StrId,
    /// Version of a package we're currently compiling.
    pub version: Version,
    /// Enabled features for this package.
    pub features: Vec<FeatureName>,
    #[serde(rename = "path")]
    /// Path to the source directory of this package.
    pub path_to_the_src_directory: PathBuf,
    /// Dependencies of this package.
    pub dependencies: Vec<Dependency>,
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
/// A single package's dependency, in a duckc-friendly format.
pub struct Dependency {
    #[serde(rename = "name")]
    /// ID of a package. Note, that there must a package with `id = self.id` in a [`packages`](DuckcMultiPackage::packages) vector.
    pub id: StrId,
    #[serde(rename = "alias")]
    /// How should this package be named, when resolving imports.
    pub import_name: StrId,
}

// `compiler/driver/driver/src/driver/task/task.cpp` deserializes `RawTask` as `RawPackageCompilationTask`.
// So, from JSON POV, they are the same type.
pub type Task = PackageCompilationTask;

#[derive(Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
pub struct PackageCompilationTask {
    #[serde(rename = "package")]
    /// ID of a package refered by this task.
    pub package_id: StrId,
    #[serde(flatten)]
    /// Compilation strategy of this task.
    pub strategy: PackageCompilationStrategy,
}

#[derive(Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
// These and the flatten above are required for generating valid JSON's, while still giving us type safety :^).
#[serde(rename_all = "snake_case", tag = "strategy")]
/// Supported duckc compilation strategies.
pub enum PackageCompilationStrategy {
    /// Compile this task into a DVM file.
    // Name has to be `Dvm`, as `DVM` (with "snake_case") would be rendered as "d_v_m".
    Dvm {
        /// Set an explicit output filename. By default `package_dvm` is used.
        #[serde(skip_serializing_if = "Option::is_none")]
        output_file: Option<PathBuf>,
    },
    Native {
        /// Path to the output file.
        output_file: PathBuf,
        /// Additional linking options.
        #[serde(skip_serializing_if = "Option::is_none")]
        linking_options: Option<LinkerOptions>,
    },
    Lib {
        /// Path to the output file.
        output_file: PathBuf,
        /// Additional archiving options.
        #[serde(skip_serializing_if = "Option::is_none")]
        archive_options: Option<ArchiverOptions>,
    },
}

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
/// Possible variants of linking options passed to duckc.
pub enum LinkerOptions {
    /// Add extra linker arguments.
    RawLinkerArgs(String),
    Complex(ComplexLinkerOptions),
}

impl ser::Serialize for LinkerOptions {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        match self {
            Self::RawLinkerArgs(raw) => raw.serialize(serializer),
            Self::Complex(complex) => complex.serialize(serializer),
        }
    }
}

impl<'de> de::Deserialize<'de> for LinkerOptions {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a valid duckc linking options")
            .string(|string| Ok(Self::RawLinkerArgs(string.to_string())))
            .map(|map| map.deserialize().map(Self::Complex))
            .deserialize(deserializer)
    }
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
/// More complex linking options.
pub struct ComplexLinkerOptions {
    /// Use an explicit linker, instead of the system default.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub linker: Option<PathBuf>,
    /// Pass additional linker arguments.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub additional_linking_options: Option<String>,
    #[serde(
        rename = "link_c_standard_library",
        skip_serializing_if = "Option::is_none"
    )]
    /// Whether or not link C Std.
    pub link_cstd: Option<bool>,
}

#[derive(Debug, Clone, Eq, PartialEq, Hash)]
/// Possible variants of archiver options passed to duckc.
pub enum ArchiverOptions {
    /// Set an explicit archiver path.
    Archiver(PathBuf),
    Complex(ComplexArchiverOptions),
}

impl ser::Serialize for ArchiverOptions {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: ser::Serializer,
    {
        match self {
            Self::Archiver(raw) => raw.serialize(serializer),
            Self::Complex(complex) => complex.serialize(serializer),
        }
    }
}

impl<'de> de::Deserialize<'de> for ArchiverOptions {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: de::Deserializer<'de>,
    {
        serde_untagged::UntaggedEnumVisitor::new()
            .expecting("a valid duckc archiver options")
            .string(|string| Ok(Self::Archiver(string.into())))
            .map(|map| map.deserialize().map(Self::Complex))
            .deserialize(deserializer)
    }
}

#[derive(Default, Debug, Clone, Serialize, Deserialize, Eq, PartialEq, Hash)]
/// More complex archiver options.
pub struct ComplexArchiverOptions {
    /// Set an explicit archiver.
    #[serde(skip_serializing_if = "Option::is_none")]
    pub archiver: Option<PathBuf>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn basic_json_convertion() {
        let packages = create_mock_packages();
        let tasks = create_mock_tasks();
        let multi_package = MultiPackage { packages, tasks };
        let serialized = serde_json::to_string_pretty(&multi_package).unwrap();
        assert_eq!(
            serialized,
            r#"{
  "packages": [
    {
      "name": "a",
      "version": "1.0.0",
      "features": [],
      "path": "",
      "dependencies": [
        {
          "name": "dep-hash",
          "alias": "bar"
        },
        {
          "name": "dep2-hash",
          "alias": "foo"
        }
      ]
    },
    {
      "name": "dep-hash",
      "version": "2.0.0",
      "features": [
        "feature"
      ],
      "path": "",
      "dependencies": [
        {
          "name": "dep2-hash",
          "alias": "foo-aliased"
        }
      ]
    },
    {
      "name": "dep2-hash",
      "version": "2.1.37",
      "features": [
        "foo",
        "bar"
      ],
      "path": "",
      "dependencies": []
    }
  ],
  "tasks": [
    {
      "package": "a",
      "strategy": "native",
      "output_file": "out.exe",
      "linking_options": {
        "additional_linking_options": "-lfoo",
        "link_c_standard_library": true
      }
    },
    {
      "package": "a",
      "strategy": "native",
      "output_file": "out.exe",
      "linking_options": "-lfoo"
    },
    {
      "package": "a",
      "strategy": "native",
      "output_file": "out.exe"
    },
    {
      "package": "dep-hash",
      "strategy": "dvm",
      "output_file": "out.dvm"
    },
    {
      "package": "dep-hash",
      "strategy": "dvm"
    },
    {
      "package": "dep2-hash",
      "strategy": "lib",
      "output_file": "out.so",
      "archive_options": "ar"
    },
    {
      "package": "dep2-hash",
      "strategy": "lib",
      "output_file": "out.so",
      "archive_options": {
        "archiver": "ar"
      }
    },
    {
      "package": "dep2-hash",
      "strategy": "lib",
      "output_file": "out.so",
      "archive_options": {}
    },
    {
      "package": "dep2-hash",
      "strategy": "lib",
      "output_file": "out.so"
    }
  ]
}"#
        );

        let deserialized = serde_json::from_str::<MultiPackage>(&serialized).unwrap();
        assert_eq!(deserialized, multi_package);
    }

    fn create_mock_packages() -> Vec<Package> {
        vec![
            Package {
                id: "a".into(),
                version: Version::new(1, 0, 0),
                features: vec![],
                path_to_the_src_directory: PathBuf::default(),
                dependencies: vec![
                    Dependency {
                        id: "dep-hash".into(),
                        import_name: "bar".into(),
                    },
                    Dependency {
                        id: "dep2-hash".into(),
                        import_name: "foo".into(),
                    },
                ],
            },
            Package {
                id: "dep-hash".into(),
                version: Version::new(2, 0, 0),
                features: vec!["feature".into()],
                path_to_the_src_directory: PathBuf::default(),
                dependencies: vec![Dependency {
                    id: "dep2-hash".into(),
                    import_name: "foo-aliased".into(),
                }],
            },
            Package {
                id: "dep2-hash".into(),
                version: Version::new(2, 1, 37),
                features: vec!["foo".into(), "bar".into()],
                path_to_the_src_directory: PathBuf::default(),
                dependencies: vec![],
            },
        ]
    }
    fn create_mock_tasks() -> Vec<PackageCompilationTask> {
        vec![
            PackageCompilationTask {
                package_id: "a".into(),
                strategy: PackageCompilationStrategy::Native {
                    output_file: PathBuf::from("out.exe"),
                    linking_options: Some(LinkerOptions::Complex(ComplexLinkerOptions {
                        linker: None,
                        additional_linking_options: Some("-lfoo".into()),
                        link_cstd: Some(true),
                    })),
                },
            },
            PackageCompilationTask {
                package_id: "a".into(),
                strategy: PackageCompilationStrategy::Native {
                    output_file: PathBuf::from("out.exe"),
                    linking_options: Some(LinkerOptions::RawLinkerArgs("-lfoo".into())),
                },
            },
            PackageCompilationTask {
                package_id: "a".into(),
                strategy: PackageCompilationStrategy::Native {
                    output_file: PathBuf::from("out.exe"),
                    linking_options: None,
                },
            },
            PackageCompilationTask {
                package_id: "dep-hash".into(),
                strategy: PackageCompilationStrategy::Dvm {
                    output_file: Some(PathBuf::from("out.dvm")),
                },
            },
            PackageCompilationTask {
                package_id: "dep-hash".into(),
                strategy: PackageCompilationStrategy::Dvm { output_file: None },
            },
            PackageCompilationTask {
                package_id: "dep2-hash".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("out.so"),
                    archive_options: Some(ArchiverOptions::Archiver("ar".into())),
                },
            },
            PackageCompilationTask {
                package_id: "dep2-hash".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("out.so"),
                    archive_options: Some(ArchiverOptions::Complex(ComplexArchiverOptions {
                        archiver: Some("ar".into()),
                    })),
                },
            },
            PackageCompilationTask {
                package_id: "dep2-hash".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("out.so"),
                    archive_options: Some(ArchiverOptions::Complex(ComplexArchiverOptions {
                        archiver: None,
                    })),
                },
            },
            PackageCompilationTask {
                package_id: "dep2-hash".into(),
                strategy: PackageCompilationStrategy::Lib {
                    output_file: PathBuf::from("out.so"),
                    archive_options: None,
                },
            },
        ]
    }
}
