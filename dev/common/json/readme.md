\page json-module JSON Module

@warning JSON is deprecated.
JSON was added by ZPP 1.1 and uses external library. It was concluded that the external library is buggy or behaves strangely. 

# JSON module documentation

## Library json_struct
Source and documentation:
https://github.com/jorgen/json_struct
## Basic usage
### Type names
JSON module provides a way to register name aliases to C++ types in order to serialize them
```c++
REGISTER_PARSE_TYPE(Object)
```
or with an alias
```c++
REGISTER_PARSE_TYPE_ALIAS(Object, "Alias")
```

Templated objects can also be aliased:
```c++
REGISTER_PARSE_TYPE_TEMPLATE_ALIAS(std::vector, "vector")
```

of for variadic templates

```c++
REGISTER_PARSE_TYPE_TEMPLATE_VARIADIC_ALIAS(std::tuble, "tuple")
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

json_struct can parse JSON and automatically populate structures with content by adding some metadata to the C++ structs.

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

    JS_OBJ(one, two, three);
};
```

or

```c++
struct JsonObject {
    int one;
    std::string two;
    double three;
};
JS_OBJ_EXT(JsonObject, one, two, three);
```

or for empty structs:

```c++
struct JsonEmptyObject {};
JS_EMPTY(Object)
```

Populating the struct would look like this:

```c++
JS::ParseContext context(json_data);
JsonObject obj;
context.parseTo(obj);
```

Serializing the struct to JSON could be done like this:

```c++
std::string pretty_json = JS::serializeStruct(obj);
// or
std::string compact_json = JS::serializeStruct(obj, JS::SerializerOptions(JS::SerializerOptions::Compact));
```

### Variants
For now, only serialization of `std::variant<Args...>` is prepared, where each `Arg`$\in$`Args` is either a struct serialized with `JS_OBJ` or `JS_OBJ_EXT` or `JS_EMPTY`, or an empty struct. Each `Arg` must have a registered type as described above.
> Arg CANNOT be one of std::string, std::variant, std::unique_ptr, table, nor any primitive type

### Predefined serialization
The following types have predefined serialization in json_struct
- std::string
- double
- float
- uint8_t
- int16_t
- uint16_t
- int32_t
- uint32_t
- int64_t
- uint64_t
- std::unique_ptr
- bool
- std::vector
- [T]
### Custom serialization
You can implement custom serialization by implementing `TypeHandler` class. For example:
```c++
namespace JS {
template<>
struct TypeHandler<uint32_t> {
public:
    static inline Error to(uint32_t &to_type, ParseContext &context) {
        char *pointer;
        unsigned long value = strtoul(context.token.value.data, &pointer, 10);
        to_type = static_cast<unsigned int>(value);
        if (context.token.value.data == pointer)
            return Error::FailedToParseInt;
        return Error::NoError;
    }

    static void from(const uint32_t &from_type, Token &token, Serializer &serializer) {
        std::string buf = std::to_string(from_type);
        token.value_type = Type::Number;
        token.value.data = buf.data();
        token.value.size = buf.size();
        serializer.write(token);
    }
};
}
```
