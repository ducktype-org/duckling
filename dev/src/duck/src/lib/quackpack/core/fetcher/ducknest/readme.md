# Registry communication

At a time of writing this file, we communicate with a registry instance using a REST API,
backed by cURL-based [`HttpClient`](../http/mod.rs).

In a future, we'll improve it with cURLs [multi interface](https://curl.se/libcurl/c/libcurl-multi.html) for parallel requests.

## An overview of Ducknest endpoints

### Fetching package's data

#### Metadata (manifest)

##### GET `/packages/{name}`

Get all metadata (manifests) of this package

Returns [`MultiMetadata`](../types.rs) as JSON

##### GET `/packages/{name}/{version}`

Get metadata (manifest) of that package in the specified version

Returns [`registry::Manifest`](../../../schemas/registry.rs) as JSON

#### Blobs

##### GET `/packages/{name}/{version}/download`

Returns a bytes, which represent a tar gzip'ed source of this package

### Searching packages

#### GET `/packages?q={query}`

Search all packages by a given query

Returns [`SearchResult`](../types.rs) as JSON

### Publishing a packages

@TODO: #1905 Change that

#### POST `/packages`

Create a new package in the registry

Body: [`registry::Manifest`](../../../schemas/registry.rs) as JSON

#### PUT `/packages/{name}/{version}`

Upload a tar gzip'ed source of a package to the registry

It uses forms and/or multiparts
