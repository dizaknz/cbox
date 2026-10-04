# Deterministic, lightweight, and completely allocation-free C++ example 

# Build

```
cmake -B build -S .
cmake --build build
```

# Tests

```
cd build && ctest --output-on-failure
```

# 🚀 Architectural Highlights

This design moves typical runtime performance bottlenecks directly to the compiler, enforcing type safety and performance entirely on the stack.

- Allocation-Free Vocab Types (std::string_view & std::span)
  - The Benefit: Data paths do not copy bytes or allocate memory on the heap.
  - How it works: ConnectionText uses std::string_view to reference read-only string data baked into the binary. ConnectionDataPacket uses a mutable std::span<std::byte> to safely inspect and mutate raw, stack-allocated byte arrays in place, tracking sizes without container allocations.
- Advanced Variant Mechanics (Dual Dispatch & Nested Unpacking)
  - Dual Dispatching: By passing two variants to std::visit, the compiler generates a flat compile-time branch matrix to resolve the combination of two active types simultaneously (e.g., executing unique code when a ConnectionDataPacket meets a ConnectionText).
  - Nested Unpacking: When handling a ConnectionComplexFailure (which contains a nested variant), the visitor uses a secondary nested std::visit combined with if constexpr. This extracts the deep inner state safely without runtime class inheritance or heap-allocated polymorphism.
- Compile-Time Guards (C++20 Concepts & consteval)
  - Format-String Validation: The ValidatedFormatString class utilizes a C++20 consteval constructor. If a developer accidentally passes a logging pattern without formatting braces ({}), the compilation fails automatically, eliminating hidden logging bugs before the code compiles.
  - Variant Constrained Fallbacks: A template fallback operator() acts as a catch-all. It is secured by a custom C++20 concept that queries std::variant_size_v. If the master variant changes or someone tries to route a large, non-trivially copyable type into the generic fallback, the compiler rejects the build.
- Functional Short-Circuiting (C++23 std::expected Monads)
  - The Benefit: Complete removal of expensive try/catch exception runtime overhead.
  - How it works: The logging configuration pipelines are built sequentially using .and_then() and .or_else(). If any initialization phase fails, the pipeline immediately short-circuits to an error payload using a type-safe stack union, preserving absolute performance determinism.
- Clean Mechanics (C++20 Designated Initializers & CRTP)
  - Designated Initializers: The code constructs configurations via precise .field = value assignments, improving readability and protecting the codebase from parameter ordering breakages during future updates.
  - Static Polymorphism (CRTP): The visitor inherits from BaseProcessor<StateProcessor>, allowing compile-time telemetry and boilerplate tracking hooks to execute without paying the performance penalty of a traditional virtual table (vtable).


