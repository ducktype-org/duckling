# JSON Module

Wraps `nlohmann::json` with project-specific helpers:

Underlying library: <https://github.com/nlohmann/json>.

## Choosing an API

- **Use `diagnostics.hpp`** when you want failures to be surfaced as
  user-facing diagnostics (manifest parsing, config files, REPL input).
  Each helper takes a `DiagnosticLogger` callback and reports on
  missing/wrong-typed fields.
- **Use `extract.hpp`** when you want to inspect outcomes silently —
  e.g. probing whether a field is present, or accumulating multiple
  errors yourself before deciding what to log. Returns `std::expected<T, JsonExtractError>`.
- **Use `NLOHMANN_DEFINE_TYPE_INTRUSIVE` / `adl_serializer`** for
  symmetric round-trip serialization of plain data structs.

The diagnostic helpers are built on top of the pure ones — picking the
right layer keeps log policy out of low-level parsing code.

### Typical parser shape

```cpp
base::Optional<MyThing> MyThing::fromJson(
    const nlohmann::json& json, const js::DiagnosticLogger& report
) {
    if (!js::checkIsObject(json, "my_thing", report)) return {};

    js::checkForUnknownFields(
        json, /*required*/ { "name" }, /*optional*/ { "alias" },
        "my_thing", report
    );

    bool had_error = false;

    auto name = js::getString(json, "name", "my_thing requires a name!", report);
    if (!name) had_error = true;

    base::Optional<base::StrID> alias;
    if (auto v = js::extractString(json, "alias"); v.has_value())
        alias = *v;

    if (had_error) return {};

    return MyThing{ .name = *name, .alias = alias.value_or(base::StrID{}) };
}
```

Pattern: collect errors with a `had_error` flag, return after the whole
object is parsed — that way the user sees every diagnostic in one pass
instead of one-at-a-time.

## `extract.hpp` — pure helpers

```cpp
enum class JsonExtractError { NotAnObject, MissingKey, WrongType };

bool isObject(const nlohmann::json& j) noexcept;

std::expected<base::StrID, JsonExtractError>            extractString(j, key);
std::expected<bool, JsonExtractError>                   extractBool(j, key);
std::expected<std::vector<nlohmann::json>, …>           extractArray(j, key);
std::expected<nlohmann::json, …>                        extractObject(j, key);
std::expected<base::StrID, JsonExtractError>            extractStringValue(j);

std::vector<std::string> findUnknownFields(j, required, optional);
```

No logging, no allocations beyond what the result requires. Use them
when the diagnostic-aware variants do not fit (e.g. truly optional
fields where "missing" is not interesting).

## Type names (`type_parse.hpp`)

Register an alias for variant serialization or human-readable type tags:

```cpp
JSON_REGISTER_TYPE(Object)
JSON_REGISTER_TYPE_WITH_NAME(Object, "Alias")
JSON_REGISTER_TEMPLATE_WITH_NAME(std::vector, "vector")
JSON_REGISTER_TEMPLATE_VARIADIC_WITH_NAME(std::tuple, "tuple")
```

Custom names via `TypeParseTraits` (uses `CONSTEXPR_CAT` for compile-time
concat):

```cpp
template<class T>
struct TypeParseTraits<T[]> {
    static constexpr const auto name = CONSTEXPR_CAT("[", TypeParseTraits<T>::NAME, "]");
};
```

## Structs

`nlohmann::json` round-trips structs declared with the intrusive macro:

```cpp
struct JsonObject {
    int         one;
    std::string two;
    double      three;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(JsonObject, one, two, three);
};
```

```cpp
JsonObject     obj  = nlohmann::from_json(json_data);
nlohmann::json out  = obj;
```

## Variants

Serialization of `std::variant<Args...>` is supported provided each
`Arg` is registered with `JSON_REGISTER_TYPE` / `…_WITH_NAME`.
Deserialization is not yet wired up.

## Custom serializers

Specialize `nlohmann::adl_serializer` for full control:

```cpp
template<>
struct nlohmann::adl_serializer<uint32_t> {
    static void to_json(json& j, const uint32_t& e) {
        j["value"]      = e;
        j["field_name"] = "extra metadata if you want it";
    }
    static void from_json(const json& j, uint32_t& value) {
        value = std::stoul(j["value"].get<std::string>());
    }
};
```
