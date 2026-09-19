## Differences between `Source`, `Identity`, and `FullIdentity`

As You might have noticed, these three types look very similar, yet, fundamentally, they are different,
and each has a different responsibility.

Firstly, let's talk why `Source` is different from `Identity` and `FullIdentity`.
`Source` comes from manifest parsing, thus it holds a lot of information acquired there;
in particular, it has `GitReference`, a structure telling us, how to clone git repositories.

During solving, gatherer will clone those repositories, and later refer to them only by `Url` + resolved `commit`.
In fact, in many places that come after solving (storage, compilation), we do not use `Source`s, at all.

That's why we have `Identity` (and `FullIdentity`); firstly, their names tell us that they __are__ unique in the resolved dependencies' graph (which is not true for `Source`s, as we could have a `GitReference` pointing at the default branch, and another, pointing explicitly at `branch: main`).

> [!IMPORTANT]
> In the build graph/freeze, `Identity` has to be a unique identifier. However, `FullIdentity`
> is _not a unique identifier. Many `FullIdentity`ies can map to the same `Identity`. `FullIdentity`
> is needed only to store resolved git hash.

Now let's talk about differences between `Identity` and `FullIdentity`.
Firstly, they are differences in how these structures behave when (de)-serializing.
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
  "version": "1.0.0",
  "features": [],
  "dependencies": [
     "foo registry+https://duckling_registry.com/",  // <- This string is an `Identity`,
  ],
}
```

_Full packages_ entries take a `FullIdentity`, in order to describe their sources.
However, when talking about their _dependencies_, we use a bit more lightweight `Identity` to point at another package.
