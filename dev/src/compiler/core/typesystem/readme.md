\page typesystem-module Type System

The Type System is broadly responsible for handling information about types. It is divided into two parts, the Higher Type System, and the Lower Type System.

## Higher Type System

The Higher Type System is responsible for handling theoretical and semantic information about the types. This includes, but is not limited to:

- type construction,
- type identification,
- value categories,
- interfaces, and
- implicit coercibility.

You should be interested in the Higher Type System if you want to consider types in a similar way to how a (non-embedded) programmer typically thinks about them, i.e. in terms of their meaning, but not necessarily precise representation in memory.

Importantly, the Higher Type System should provide all necessary information about a type for further compilation processes.

The Higher Type System cares greatly to deduplicate its data, because precise type identification is required.

\todo More details about the components (TypeInfo, ValueCategory, etc.) in other pages.

## Lower Type System

The Lower Type System is responsible for generating precise runtime in-memory type representation. This includes, but is not limited to:

- type layouts, including:
  - packing,
  - alignment,
  - size,
- vtable design, and
- design of pointer implementation.

The Lower Type System depends on the Higher Type System. It generates its data based on the information provided by its counterpart.

It is not crucial for the Lower Type System to deduplicate its data. However, it is important for the same layouts to be generated for the same types. In other words, the Lower Type System does not have to deduplicate its data, but the generated data must be consistent and, in a sense, confluent.

\todo More details about components (names unknown) in other pages.