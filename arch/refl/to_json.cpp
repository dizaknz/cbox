#include <iostream>
#include <string_view>
#include <tuple>
#include <type_traits>

// conversion helper to match any type implicitly
struct AnyType {
    template <typename T> operator T();
};

// detect structural aggregate field count
template <typename T, typename... Args> constexpr auto count_fields() {
    if constexpr (requires { T{{Args{}}..., {AnyType{}}}; }) {
        return count_fields<T, Args..., AnyType>();
    } else {
        return sizeof...(Args);
    }
}

template <typename T> constexpr auto to_tuple(T &&obj) {
    using Decayed = std::decay_t<T>;
    constexpr size_t fields = count_fields<Decayed>();
    // ladder supports up to 4 fields, structured bindings (mutable) -> tuple
    if constexpr (fields == 1) {
        auto &[a] = obj;
        return std::forward_as_tuple(a);
    } else if constexpr (fields == 2) {
        auto &[a, b] = obj;
        return std::forward_as_tuple(a, b);
    } else if constexpr (fields == 3) {
        auto &[a, b, c] = obj;
        return std::forward_as_tuple(a, b, c);
    } else if constexpr (fields == 4) {
        auto &[a, b, c, d] = obj;
        return std::forward_as_tuple(a, b, c, d);
    } else {
        return std::forward_as_tuple();
    }
}

// compile-time print driver
void print_value(const auto &val) {
    if constexpr (
        std::is_same_v<std::decay_t<decltype(val)>, std::string_view> ||
        std::is_same_v<std::decay_t<decltype(val)>, const char *>
    ) {
        std::cout << "\"" << val << "\"";
    } else {
        std::cout << val;
    }
}

// reflection loop
template <typename T> void serialize_to_json(const T &obj) {
    auto tuple_representation = to_tuple(obj);

    std::cout << "{ ";
    std::apply(
        [](const auto &...args) {
            size_t index = 0;
            auto print_pair = [&](const auto &val) {
                std::cout << "\"field_" << index++ << "\": ";
                print_value(val);
                if (index < sizeof...(args))
                    std::cout << ", ";
            };
            (print_pair(args), ...);
        },
        tuple_representation
    );

    std::cout << " }\n";
}

struct OrderPacket {
    uint32_t order_id;
    double price;
    const char *ticker;
    uint32_t quantity;
};

int main() {
    constexpr size_t fields = count_fields<OrderPacket>();
    static_assert(fields == 4, "Static reflection engine failed structural validation constraint.");
    OrderPacket tx_order{94821, 142.50, "AAPL", 500};

    std::cout << "Detected Fields: " << fields << "\n";
    std::cout << "JSON Payload:    ";
    serialize_to_json(tx_order);
}
