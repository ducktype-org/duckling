\page json-module JSON Module

# JSON module documentation

## Library nlohmann::json
Source and documentation:
https://github.com/nlohmann/json

## Basic usage
### Type names
JSON module provides a way to register name aliases to C++ types in order to serialize them
```c++
JSON_REGISTER_TYPE(Object)
```
or with an alias
```c++
JSON_REGISTER_TYPE_WITH_NAME(Object, "Alias")
```

Templated objects can also be aliased:
```c++
JSON_REGISTER_TEMPLATE_WITH_NAME(std::vector, "vector")
```

of for variadic templates

```c++
JSON_REGISTER_TEMPLATE_VARIADIC_WITH_NAME(std::tuple, "tuple")
```

Custom type names can be achieved by implementing `TypeParseTraits`, for example for tables:
```c++
template<class T>
struct TypeParseTraits<T[]> {
	static constexpr const auto name = CONSTEXPR_CAT("[", TypeParseTraits<T>::name, "]");
};
```

where `CONSTEXPR_CAT` concatenates string in compile time.

### Structs

nlohmann::json can parse JSON and populate structures with content.

```json
{
    "one" : 1,
    "two" : "two",
    "three" : 3.333
}
```

can be parsed into a structure defined like this:

```c++
struct JsonObject {
    int one;
    std::string two;
    double three;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(JsonObject, one, two, three);
};
```

Populating the struct would look like this:

```c++
JsonObject object = nlohmann::from_json(json_data);
```

Serializing the struct to JSON could be done like this:

```c++
JsonObject obj = { ... };
nlohmann::json json_obj = obj;  // Simple as that
std::cout << json_obj << '\n';  // It is very flexible
```

### Variants
For now, only serialization of `std::variant<Args...>` is prepared, where each `Arg`$\in$`Args` is registered with `JSON_REGISTER_TYPE(_WITH_NAME)`. 

### Serialization
Most standard types have a predefined serialization, but you can have custom serialization by specializing `adl_serializer` struct. For example:
```c++
template<>
struct nlohmann::adl_serializer<uint32_t> {
    static void to_json(json& j, const uint32_t& e) {
        j["value"] = uint32_t;
        j["field_name"] = "This is how simple it is!";  // Some additional data if you want
    }

    static void from_json(const json& j, uint32_t& value) {
        value = std::stoul(j["value"]);
    }
};
```
