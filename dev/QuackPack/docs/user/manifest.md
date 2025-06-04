# Project configuration

Every project is configured through its *manifest*.
Each manifest points to the project's root, and should be named `quackconfig.yml`.

Below is example of full project's manifest.

```yaml
metadata:
  name: Cool_package_name
  version: 2.1.37
  author: Elitarny MIM
  description: Very cool package for MIM
  license: GLWTS
  vm_version: 2.71.0
  compiler_version: 3.14
  is_temporary: False

dependencies:
  version_list_dep:
    version: 1.2.3 or 4.1
    conditions:
      system: [Windows, Mac]
      arch: [x86-64, arm64]
    flags:
    - a:
        system: i386
  local_dep:
    version:
      path: ~/src/local_dependency
  git_dep:
    version:
      git_url: https://github.com/my_nick/my_repo.git
      branch: stable
  flags_with_conditions:
    version: 2.1.9
    flags: [ a, {b: {arch: ARM}}, c, d, e, f, g, h]
    conditions:
      system: Windows
      arch: ARM
      project_flags: [x, y, z]

dev_dependencies:
  ducktest:
    version: 1

features:
  cool_feature: []
  middle_feature: [cool_feature]
  top_level: [middle_feature]

targets:
  i686-linux-gnu:
    compiler_flags: [-O2]

profiles:
  debug:
    compiler_flags: [-O0, -ggdb3]
```

### Short note on versions

We use [SemVer 2.0](https://semver.org/spec/v2.0.0.html) as a versioning system with one small addition: `minor` and `patch` can be skipped, and they default to 0.
That means version `1` is the same as `1.0` and `1.0.0`, and `1.4` is the same as `1.4.0`.

## Metadata

This is the only obligatory section of the manifest. It contains following keys:

- `name`: this is project's name; it's used as an identifier in the code and for searching. Should be a valid Duckling identifier. It is one of the two obligatory keys.
- `version`: this denotes current project's version. It is second obligatory key.
- `author`: [OPTIONAL] project's author. **FIXME:** right now we support only single author, add support for lists.
- `description`: [OPTIONAL] short project's description. It's visible on [**FIXME:** insert here default ducknest URL].
- `license`: [OPTIONAL] license used in the project. Note, that it is **REQUIRED**, when project is about to be published.
- `vm_version`: [OPTIONAL] PondVM version this project runs on.
- `compiler_version`: [OPTIONAL] compiler version required for this project. **FIXME:** replace `vm_version` and `compiler_version` with something like `language_edition`.
- `is_temporary`: [OPTIONAL] should this virtual environment be treated as a temporary virtual environment. When publishing, it is **REQUIRED** to set this to `false`.

## Dependencies and dev dependencies
Each top-level key is a different dependency. 

### Single dependency keys

- `version`: it is the only required key. It specifies, whether dependency should be downloaded from the Ducknest, should be cloned from the git, or is a local dependency (living on a disk).

#### Ducknest source

When using dependency from a Ducknest (default) `version` should be either a list of versions or a *version list*.

*Version list* allows to specify multiple different versions by using `or` word as a delimiter.
In particular, `version: 1.2.3` is a single entry *version list*, whereas `version: [1.2.3]` is a single entry list of versions.
Bigger example is in the example manifest above, the `version_list_dep` dependency.

#### Git source

In the case of git, `version` should be a dictionary with following keys:

- `git_url`: URL of a git repository with source of the dependency. It is required.
- `commit`: [OPTIONAL] if specified, use the exact provided commit hash.
- `branch`: [OPTIONAL] if specified, use provided branch instead of the default one.
- `tag`: [OPTIONAL] if specified, use provided tag instead of the default latest commit.

**FIXME:** co się z czym wyklucza.

#### Local source

When using local dependencies, `version` should be a dictionary with single key:

- `path`: should point to the root directory of a dependency. Tildes (`~`) are allowed.

**FIXME:** Relatywne ścieżki ok (względem root obecnego projektu), czy nie ok?

### Dependency conditions and flags

#### Dependency flags

It is a list of flags required from specified dependency.

Every flag can also come with *conditions*, which needs to be fulfilled for a flag to be turned on.
In that case, flag name should be a key in the dictionary, as shown in  `flags_with_conditions` in the example manifest above.

*Conditions* are the same as below.

#### Dependency conditions

It contains three keys:

- `system`: [OPTIONAL] host system required for specific dependency (or a dependency flag, as explained above),
- `arch`: [OPTIONAL] CPU architecture required for specific dependency,
- `project_flags`: [OPTIONAL] only use specified dependency when building **your** project with specified flags.

## Features

Features exposed by your project.
Each key is a feature name, and value is a list (may be empty) of **your** features required for specific features.
They are intrusive (FIXME: dobre słowo xD?) meaning, that in example above, turning on `top_level` also turns on `middle_feature` and `cool_feature`.

## Targets and profiles

Those sections enable extra compiler flags, when building project on specified CPU or with specified profile.
They are both dictionaries, with keys being:

- target name, in case of `targets`,
- profile name, in case of `profiles`.

Each name should be a dictionary with single key:

- `compiler_flags`: list of extra compiler flags to be append.

### NOTE

Any profile name is a valid profile, but targets are more restricted, to the following list: FIXME.
