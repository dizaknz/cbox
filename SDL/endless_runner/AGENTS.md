# AGENTS.md for Modern C (C23)

1. Core Guidelines

- Target Standard: C23 (`__STDC_VERSION__ >= 202311L`).
- Safety First: Prefer safe library functions (`_s` suffixes) over unsafe ones.
- Modernity: Use true/false (no `<stdbool.h>` needed), `nullptr`, and `static_assert`.
- Clarity: Use auto for variable type inference where appropriate. 

2. Coding Conventions & Syntax

- Boolean/Null: Use `bool` (instead of `int` or `<stdbool.h>`) and `nullptr` (instead of `NULL` or 0).
- Initialization: Favor `struct { .a = 1, .b = 2 };` for designated initializers.
- Attributes: Use [[nodiscard]] for functions that shouldn't be ignored and `[[maybe_unused]]` to suppress warnings.
- Bit-precise integers: Use `_BitInt(N)` for custom-width integers (e.g., `_BitInt(128)`) when high precision is needed without overhead.
- Lambdas: Use limited lambda expressions where syntax allows to keep code local. 

3. Mandatory C23 Features to Use

- constexpr: Replace `#define` constants and `const` variables with `constexpr` for true compile-time constant evaluation.
- auto type inference: Use `auto x = expression;` to reduce verbosity.
- Enumerations: Explicitly specify the underlying type for enums (e.g., `enum E : int { A, B };`).
- printf specifiers: Use %zu for `size_t` and proper specifiers for bit-precise types. 

4. Prohibited/Deprecated Practices

- NO: K&R function declarations.
- NO: `gets()` or unsafe `strcpy`.
- NO: `void*` pointer arithmetic without casting.
- AVOID: long long when `_BitInt` or fixed-width types (stdint.h) are more appropriate. 

5. Build/Development Tasks

- Compiler Flag: Always compile with `-std=c23` (or `-std=c2x` on older GCC/Clang).
- Warnings: Enable `-Wall -Wextra -Wpedantic -Werror`.
- Headers: Include `<stdbool.h>` (for compatibility, though mostly implicit), `<stddef.h>`, and `<stdint.h>`
