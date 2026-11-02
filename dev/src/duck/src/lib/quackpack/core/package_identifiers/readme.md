# Differences between `Source`, `Identity`, `FullIdentity`, and `PackageId`

As You might have noticed, these three types look very similar, yet, fundamentally, they are different,
and each has a different responsibility.

Firstly, let's talk why `Source` is different from `Identity` and `FullIdentity`.
`Source` comes from manifest parsing, thus it holds a lot of information acquired there;
in particular, it has `GitReference`, a structure telling us, how to clone git repositories.

During solving, gatherer will clone those repositories, and later refer to them only by `Url` + resolved `commit`.
In fact, in many places that come after solving (storage, compilation), we do not use `Source`s, at all.

That's why we have `Identity`: firstly, their names tell us that they __are__ unique in the resolved
dependencies' graph (which is not true for `Source`s, as we could have a `GitReference` pointing at
the default branch, and another pointing explicitly at `branch: main`).

## `Identity`, `FullIdentity`, and `PackageId`

> [!IMPORTANT]
> If you want to validate a build graph (f.e. check that there are no duplicates) use `Identity` for that.
> `FullIdentity` exists for a different reason.

Now let's talk about differences between `Identity` and `FullIdentity`.

`Identity` is a unique identifier of a package in a build/solver graph.
However, it does _not_ store commits or versions.
That's why there are two different types:
* `FullIdentity`: `Identity` + commit for git packages,
* `PackageId`: `FullIdentity` + version of a package.
They are mainly used by solver and storage, in freezefiles.

If you'll look at the `VenvFreeze` type, you'll notice that dependencies
(in sense “packages which are required”/top-level entries) contain (flattened) `PackageId`: they've
got fields `name`, `source` (which together make `FullIdentity`) and `version` (all three make `PackageId`).
However, when talking about dependencies' of a package (lists in `VenvFreeze`) you can see that in
JSON these are strings (`Identity`s), which are unique and point at another entries.

Secondly, there are differences in how these structures behave when (de)-serializing.
This is described by the following table:

More granular differences are shown by this table:
| type           | serialization type (serde / std) | target type (JSON object / a string) | sample representation                                            | extra notes                                                                                                  |
| -------------- | -------------------------------- | ------------------------------------ | ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| `Origin`       | std                              | string                               | `"registry+https://localhost:9001/"`                             | does not store commits                                                                                       |
| `FullOrigin`   | serde                            | JSON                                 | `{"source": "registry+https://localhost:9001/"}`                 | for git sources (the ones with `git+` prefix), there will be an additional field `"commit": "<commit hash>"` |
| `Identity`     | std                              | string                               | `"name registry+https://localhost:9001/"`                        | format is `<name> <origin>`                                                                                  |
| `FullIdentity` | serde                            | JSON                                 | `{"name": "name", "source": "registry+https://localhost:9001/"}` | JSON is generally `name` + (flattened) `FullOrigin`                                                          |

Also, let's bring a sample fragment from the storage's freeze:
```json5
{
  "name": "foo",
  "source": "git+https://github.com/author/repo.git",
  "commit": "1",                                      // <- This field (and the two above) make a `FullIdentity`
  "version": "1.0.0",                                 // <- This field (and three above) make a `PackageId`
  "features": [],
  "dependencies": [
     "foo registry+https://duckling_registry.com/",  // <- This string is an `Identity`,
  ],
}
```

_Full packages_ entries take a `FullIdentity`, in order to describe their sources precisely (that's
why they store git commits). However, when talking about their _dependencies_, we use a bit more lightweight
and unique `Identity` to point at another package.
