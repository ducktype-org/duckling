## Lower Type System

The Lower Type System (TSL) is responsible for generating precise runtime in-memory type representation. This includes but is not limited to:

- type layouts, including:
    - packing,
    - alignment,
    - size,
- vtable design, and
- design of pointer implementation.

The Lower Type System depends on the Higher Type System (present as part of HELIoS). It generates its data based on the information provided by its counterpart.

It is not crucial for the Lower Type System to deduplicate its data. However, it is important for the same layouts to be generated for the same types. In other words, the Lower Type System does not have to deduplicate its data, but the generated data must be consistent and, in a sense, confluent.

@TODO: #2428 More details about components (names unknown) in other pages.