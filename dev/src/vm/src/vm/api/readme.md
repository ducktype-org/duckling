# VM API

# API internal documentation
Any client (HTTP server, terminal client) can only communicate with the VM using an API defined in a `vm.hpp`.
## Result type structure
Each API function must return a `result<T, ApiError>` type, where `T` is a successful return of the API call. `T` should be either a serializable struct (with implemented JSON serialization) or an `std::variant` with enabled JSON serialization.

`ApiError` is a `std::variant` of all possible errors.

It is done this way so the client can easily, depending on the variant alternative present in the error value, return a different message to the user.
### Adding a new error type
Either add a new type to the `ApiError` variant. One such type could be a `PreprocessorError` struct representing the information about error that could happen during the preprocessor phase.

While deciding how to add a new error type, it is crucial to think "What is the new error representing, and where it would fit best?". It is, ultimately, a subjective choice.

## IMPORTANT!!!
Each change in the API types and API calls should be done together with changes in the swagger file in the VM directory.
