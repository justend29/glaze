# Nullable and Undefinable Types

Glaze supports reading and writing both null and undefined values. To record
the presence of a null or undefined value in a struct, nullable and/or undefinable C++ types
must be used. Standard C++ types, Glaze provided types, and custom types are all available to store
null and/or undefined values. Together, nullable and/or undefinable types are referred to as nully
(null-like).

## Null vs. Undefined

Nullable and undefinable are orthogonal to eachother, allowing them to be used in any combination,
including none at all.

Traditionally, *null* is a value for a JSON field, while *undefined* indicates the value is unknown.
For dynamic types, such as a `std::map`, an undefined value would simply be omitted from the map.
However, where Glaze allows (de)serializing to/from custom C++ types, the type's members are
always present, leading to the need of separately indicating that its value was not defined.

Nullable types are those that can store a *null* value.
Undefinable types are those can can store an *undefined* value.

## Supported Types

A type being nullable or undefinable is defined by glaze using type-traits, meaning
any type that satisfies the requirements can be used to store null and/or undefined. The
following table lists common types that can be directly used

| Type               | Source | Nullable | Undefinable | Description                                 |
|--------------------|--------|----------|-------------|---------------------------------------------|
| `std::optional`    | Std    | X        |             | `null` (empty) or the contained value       |
| `std::unique_ptr`  | Std    | X        |             | `null` (empty) or the contained value       |
| `std::shared_ptr`  | Std    | X        |             | `null` (empty) or the contained value       |
| `glz::nully`       | Glaze  | X        | X           | `null`, `undefined`, or the contained value | 
| `glz::undefinable` | Glaze  |          | X           | `undefined` (empty) or the contained value  |
| `glz::nullable`    | Glaze  | X        | X           | `null` (empty) or the contained value       |
| `std::nullptr_t`   | Std    | X        |             | Always `null`                               | 
| `std::nullopt_t`   | Std    | X        |             | Always `null`                               |
| `std::monostate_t` | Std    | X        |             | Always `null`                               |
