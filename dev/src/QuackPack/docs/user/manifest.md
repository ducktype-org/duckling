# Package configuration

Every package is configured through its *manifest*.
Each manifest points to the package's root, and should be named `quackconfig.yml`.

Below is example of full package's manifest.

```yaml
metadata:
  name: Cool_package_name
  version: 2.1.37
  authors: [Elitarny MIM]
  description: Very cool package for MIM
  license: GLWTS
  language_edition: 2.71.0

dependencies:
  list:
    version: 1.2.3 or 4.1
    pinned: true
    conditions:
      system: [Windows, Mac]
      arch: [x86-64, arm64]
    features:
    - a:
        system: [i386]
  local:
    source:
      path: ~/src/local_dependency
  git:
    source:
      git_url: https://github.com/my_nick/my_repo.git
      branch: stable
  features_with_conditions:
    version: 2.1.9
    features: [ a, { b: { arch: [ARM] } }, c, d, e, f, g, h]
    conditions:
      system: [Windows]
      arch: [ARM]
      package_features: [x, y, z]
  registry:
    version: 1
    source: registry.com # uses registry.com as a registry
  registry_alias:
    version: 1
    source:
      name: foo # `registry_alias` is name used in code for `foo` package; uses default registry

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

- `name`: this is package's name; it's used as an identifier in the code and for searching. Should be a valid Duckling identifier. It is one of the two obligatory keys.
- `version`: this denotes current package's version. It is second obligatory key.
- `authors`: [OPTIONAL] package's authors. Should be a list.
- `description`: [OPTIONAL] short package's description. It's visible on [**FIXME:** insert here default ducknest URL].
- `license`: [OPTIONAL] license used in the package. Note, that it is **REQUIRED**, when package is about to be published.
- `language_edition`: [OPTIONAL] Language version required by this package.

## Dependencies and dev dependencies
Each top-level key is a different dependency. 

### Single dependency keys

There are three package types: fetched from registry, cloned from git and local dependencies, from a disk.

There are two main fields used for distinguishing between those types: `version` and `source`.

#### `version`

If this field is present, represents versions of dependency required by package.

It can be either list of SemVer version strings or a *OredSemverString*.

*OredSemverString* is a string of SemVer version strings separated using `or` word.

That is, specifying `version: [1.2.3, 2]` is the same as `version: 1.2.3 or 2`.

Both of them mean, that we require this dependency with version compatible with `1.2.3` or compatible with `2.0.0`.

#### `source`

This field specifies exact source of the dependency.

It can be a string or a dictionary.

If it is a string, it represents different registry to fetch this package from.
Otherwise this is a dictionary with following keys, each being optional string:

- `name` — global key, allowed everywhere.
- `registry_url` — key allowed only for registry sources.
- `path` — key allowed only for local sources.
- `git_url` — key allowed only for git sources.
- `branch` — key allowed only for git sources.
- `tag` — key allowed only for git sources.
- `commit` — key allowed only for git sources.

Both `source` and `version` can be omitted, but the logic behind validation is quite complex.
We'd try to explain it here.

1. For registry sources, `version` is required.
`source` is optional.
If omitted, default Ducknest URL is used.
Otherwise, it must contain only global or registry keys.
2. For local sources, `version` must be omitted. `source` must be present, and it must contain only global or local fields. In particular, `path` is required, and only allowed field.
3. For git sources, `version` may or may not be present. `source` must be present, and `git_url` field must be set. `tag`, `commit` and `branch` may or may not be set. `tag` and `branch` are mutually exclusive, but both of them are independent of `commit`.

##### `name`
It is real package name. Allows use of aliases. Using:
```yaml
foo:
  source:
    name: bar
```

means, that `foo` is an alias for `bar` — You can use `foo` in your code, but `bar` would be used by compiler.

#### About `pinned` field

This field can only be set for registry sources.
It's a boolean, which specified whether an **exact** specified version should be used.

It's similar to `=1.2.3` from Python.

### Dependency conditions and flags

#### Dependency flags

It is a list of flags required from specified dependency.

Every flag can also come with *conditions*, which needs to be fulfilled for a flag to be turned on.
In that case, flag name should be a key in the dictionary, as shown in  `features_with_conditions` in the example manifest above.

*Conditions* are the same as below.

#### Dependency conditions

It contains three keys:

- `system`: [OPTIONAL] list of host system required for specific dependency (or a dependency flag, as explained above),
- `arch`: [OPTIONAL] list of CPU architecture required for specific dependency,
- `package_flags`: [OPTIONAL] only use specified dependency when building **your** package with specified flags.

## Features

Features exposed by your package.
Each key is a feature name, and value is a list (may be empty) of **your** features required for specific features.
They are recursive meaning, that in example above, turning on `top_level` also turns on `middle_feature` and `cool_feature`.

## Targets and profiles

Those sections enable extra compiler flags, when building package on specified CPU or with specified profile.
They are both dictionaries, with keys being:

- target name, in case of `targets`,
- profile name, in case of `profiles`.

Each name should be a dictionary with single key:

- `compiler_flags`: list of extra compiler flags to be append.
