# Doxygen Documentation Guide

This guide defines how Doxygen comments are written in `dds-gamepad`. All
contributors must follow these conventions so the generated HTML is consistent
and the warning log stays clean.

## Running Doxygen

From the repository root:

```
doxygen Doxyfile
```

Output: `docs/generated/html/index.html` — open in any browser.

Check `docs/generated/doxygen-warnings.log` for coverage gaps. The log should
be empty when all documentation is complete.

> `docs/generated/` is in `.gitignore` — never commit generated HTML.

---

## Style Rules

1. Use `///` (triple-slash) for all Doxygen comments. Never use `/** */` blocks.
2. Use `@` as the tag prefix. Never use `\`.
3. Place comment blocks **immediately above** the declaration, no blank line between.
4. Every public class, struct, and enum needs both `@brief` and `@details`.
5. Every public method needs `@brief`, `@param` per parameter, and `@return`.
6. Every struct field and enum enumerator needs an inline `///<` comment.
7. Do **not** document private members.

---

## Tag Reference

| Tag                   | When to use |
|-----------------------|-------------|
| `@brief`              | One-line summary. Required on every documented symbol. |
| `@details`            | Multi-line description. Required on classes and structs. |
| `@param[in] name`     | Input parameter. |
| `@param[out] name`    | Output parameter written by the function. |
| `@param[in,out] name` | Parameter that is both read and written. |
| `@return`             | Description of the return value. |
| `@retval value`       | Named return value (e.g. `@retval true Success`). |
| `@throws ExcType`     | Exception thrown. Document every thrown type. |
| `@pre`                | Precondition the caller must satisfy before calling. |
| `@note`               | Important side note (not a warning). |
| `@warning`            | Dangerous behaviour the caller must know about. |
| `@section name Title` | Named section heading inside a class `@details`. |
| `@code` / `@endcode`  | Inline code example block. |
| `@file`               | File-level description for headers with no class/function. |
| `@copydoc sym`        | Copy documentation from another symbol (use sparingly). |

---

## Templates

### Class

```cpp
/// @brief One-line summary of the class.
///
/// @details Multi-line description. Explain:
/// - Ownership model (who creates and destroys this)
/// - Lifecycle (construction → setup → use → teardown)
/// - Thread-safety guarantees
///
/// @section usage Usage
/// @code
/// MyClass obj("name");
/// obj.DoThing(42, result);
/// @endcode
class MyClass {
public:
    /// @brief Constructs and opens the resource.
    /// @param[in] name Resource identifier.
    /// @throws std::runtime_error If the resource cannot be opened.
    explicit MyClass(const std::string& name);

    /// @brief Does the thing.
    /// @param[in]  input  Value to process.
    /// @param[out] output Receives the computed result.
    /// @return `true` on success; `false` on failure. Call `LastError()`.
    bool DoThing(int input, int& output);

    /// @brief Returns the last error message.
    /// @return Non-empty string after a failed call; empty string otherwise.
    std::string LastError() const;
};
```

### Struct

```cpp
/// @brief Brief description of the aggregate.
///
/// @details Detailed description. Explain field groups and their invariants.
struct MyStruct {
    int count = 0;        ///< Number of items. Must be >= 0.
    std::string name;     ///< Human-readable label. Required; must not be empty.
    bool enabled = true;  ///< When false the record is ignored by the engine.
};
```

### Enum

```cpp
/// @brief Describes the normalisation domain of an output channel.
///
/// @details Used by MappingEngine to apply the correct clamp range.
enum class ChannelType {
    Axis,    ///< Stick axis. Normalised to [-1.0, 1.0].
    Trigger, ///< Shoulder trigger. Normalised to [0.0, 1.0].
    Button   ///< Binary state. 1.0 = pressed, 0.0 = released.
};
```

### File-only header (no public class)

```cpp
/// @file my_header.h
/// @brief One-line description of the file's purpose.
///
/// @details What this file does, what to watch out for, and any
/// usage constraints (e.g. include order requirements).
```

---

## Doxyfile Settings That Enforce Coverage

These settings are intentional — do not change them:

| Setting | Value | Effect |
|---------|-------|--------|
| `EXTRACT_ALL` | `NO` | Only documented symbols appear in the HTML. |
| `HIDE_UNDOC_MEMBERS` | `YES` | Undocumented public members are hidden (and warned). |
| `WARN_IF_UNDOCUMENTED` | `YES` | Every undocumented public symbol produces a warning. |
| `WARN_NO_PARAMDOC` | `YES` | Missing `@param` tags produce warnings. |

The warning log is the authoritative checklist of what still needs documentation.
