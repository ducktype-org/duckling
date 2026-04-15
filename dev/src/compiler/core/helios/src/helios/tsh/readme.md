# Higher Type System

The Higher Type System (TSH) is responsible for handling theoretical and semantic information about the types. This includes but is not limited to:

- type construction,
- type identification,
- value categories,
- interfaces, and
- implicit coercibility.

You should be interested in the Higher Type System if you want to consider types similarly to how a (non-embedded) programmer typically thinks about them, i.e. in terms of their meaning, but not necessarily precise representation in memory.

Importantly, the Higher Type System should provide all necessary information about a type for further compilation processes.

The Higher Type System cares greatly to deduplicate its data because precise type identification is required.

@TODO: #2428 More details about the components (TypeInfo, ValueCategory, etc.) in other pages.