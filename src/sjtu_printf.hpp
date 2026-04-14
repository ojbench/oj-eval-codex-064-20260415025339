// Modern C++ printf with compile-time format checking
#pragma once

#include <type_traits>
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <stdexcept>
#include <ostream>
#include <iostream>

namespace sjtu {

struct format_error : std::exception {
public:
    explicit format_error(const char *msg = "invalid format") : msg_(msg) {}
    auto what() const noexcept -> const char * override { return msg_; }
private:
    const char *msg_;
};

template <typename... Args>
struct format_string {
public:
    // Construct and validate at compile time
    consteval explicit format_string(const char *fmt) : fmt_(fmt) {
        validate<Args...>(fmt_);
    }
    constexpr auto get() const noexcept -> std::string_view { return fmt_; }

private:
    static consteval bool is_spec_char(char c) {
        return c == 's' || c == 'd' || c == 'u' || c == '_';
    }

    template <typename T>
    static consteval bool is_string_like() {
        using U = std::remove_cvref_t<T>;
        return std::is_same_v<U, std::string> || std::is_same_v<U, std::string_view> ||
               std::is_same_v<U, const char *> || std::is_same_v<U, char *>;
    }

    template <typename T>
    static consteval bool is_integral_like() {
        using U = std::remove_cvref_t<T>;
        return std::is_integral_v<U>;
    }

    template <typename T>
    static consteval bool is_unsigned_like() {
        using U = std::remove_cvref_t<T>;
        return std::is_integral_v<U> && std::is_unsigned_v<U>;
    }

    template <typename T>
    static consteval bool is_signed_like() {
        using U = std::remove_cvref_t<T>;
        return std::is_integral_v<U> && std::is_signed_v<U>;
    }

    template <typename T>
    struct is_std_vector : std::false_type {};
    template <typename U, typename A>
    struct is_std_vector<std::vector<U, A>> : std::true_type {};

    template <typename... Ts>
    static consteval void validate(std::string_view fmt) {
        constexpr std::size_t N = sizeof...(Ts);
        std::size_t arg_index = 0;
        std::size_t idx = 0;
        while (idx < fmt.size()) {
            if (fmt[idx] != '%') { ++idx; continue; }
            if (idx + 1 >= fmt.size()) {
                throw format_error{"missing specifier after '%'"};
            }
            char c = fmt[idx + 1];
            if (c == '%') { idx += 2; continue; }
            if (!is_spec_char(c)) {
                throw format_error{"invalid specifier"};
            }
            if (arg_index >= N) {
                throw format_error{"too many specifiers"};
            }
            validate_at<Ts...>(arg_index, c);
            ++arg_index;
            idx += 2;
        }
        if (arg_index != N) {
            throw format_error{"too few specifiers"};
        }
    }

    template <typename T, typename... Rest>
    static consteval void validate_at(std::size_t at, char c) {
        if (at == 0) { check_one<T>(c); return; }
        if constexpr (sizeof...(Rest) == 0) {
            // out of range
            throw format_error{"too many specifiers"};
        } else {
            validate_at<Rest...>(at - 1, c);
        }
    }

    template <typename T>
    static consteval void check_one(char c) {
        if (c == 's') {
            if (!is_string_like<T>()) throw format_error{"%s requires a string"};
        } else if (c == 'd') {
            if (!is_integral_like<T>()) throw format_error{"%d requires an integer"};
        } else if (c == 'u') {
            if (!is_integral_like<T>()) throw format_error{"%u requires an integer"};
        } else if (c == '_') {
            // always ok: any type accepted by default formatting
        } else {
            throw format_error{"invalid specifier"};
        }
    }

    std::string_view fmt_{};
};

template <typename... Args>
using format_string_t = format_string<std::decay_t<Args>...>;

// Default formatting helpers
namespace detail {
    template <typename T>
    struct is_std_vector : std::false_type {};
    template <typename U, typename A>
    struct is_std_vector<std::vector<U, A>> : std::true_type {};

    template <typename T>
    constexpr bool is_string_like_v = std::is_same_v<std::remove_cvref_t<T>, std::string> ||
                                      std::is_same_v<std::remove_cvref_t<T>, std::string_view> ||
                                      std::is_same_v<std::remove_cvref_t<T>, const char *> ||
                                      std::is_same_v<std::remove_cvref_t<T>, char *>;

    template <typename T>
    void format_default(std::ostream &os, const T &val);

    inline void format_cstring(std::ostream &os, const char *p) {
        if (!p) return; // print nothing for null pointer
        os << std::string_view(p);
    }

    template <typename T>
    inline void format_integral_signed(std::ostream &os, const T &v) {
        os << static_cast<std::int64_t>(v);
    }

    template <typename T>
    inline void format_integral_unsigned(std::ostream &os, const T &v) {
        os << static_cast<std::uint64_t>(v);
    }

    template <typename T>
    inline void format_vector(std::ostream &os, const T &vec) {
        os << '[';
        bool first = true;
        for (const auto &e : vec) {
            if (!first) os << ',';
            first = false;
            format_default(os, e);
        }
        os << ']';
    }

    template <typename T>
    inline void format_default(std::ostream &os, const T &val) {
        if constexpr (is_string_like_v<T>) {
            if constexpr (std::is_same_v<std::remove_cvref_t<T>, char *>) {
                format_cstring(os, static_cast<const char *>(val));
            } else {
                os << std::string_view(val);
            }
        } else if constexpr (std::is_integral_v<std::remove_cvref_t<T>>) {
            if constexpr (std::is_signed_v<std::remove_cvref_t<T>>) {
                format_integral_signed(os, val);
            } else {
                format_integral_unsigned(os, val);
            }
        } else if constexpr (is_std_vector<std::remove_cvref_t<T>>::value) {
            format_vector(os, val);
        } else {
            // Fallback: rely on operator<< if available
            os << val;
        }
    }
} // namespace detail

template <typename... Args>
inline auto printf(format_string_t<Args...> fmt, const Args &...args) -> void {
    std::string_view s = fmt.get();
    std::size_t i = 0; // index of current argument
    auto forward_arg = [&](auto &&arg, char c) {
        using T = std::remove_cvref_t<decltype(arg)>;
        if (c == 's') {
            if constexpr (detail::is_string_like_v<T>) {
                if constexpr (std::is_same_v<T, char *>) {
                    detail::format_cstring(std::cout, static_cast<const char *>(arg));
                } else {
                    std::cout << std::string_view(arg);
                }
            } else {
                static_assert(detail::is_string_like_v<T>, "%s requires a string-like type");
            }
        } else if (c == 'd') {
            static_assert(std::is_integral_v<T>, "%d requires an integer");
            detail::format_integral_signed(std::cout, arg);
        } else if (c == 'u') {
            static_assert(std::is_integral_v<T>, "%u requires an integer");
            detail::format_integral_unsigned(std::cout, arg);
        } else if (c == '_') {
            detail::format_default(std::cout, arg);
        }
    };

    // Expand arguments into a tuple-like array for indexed access while scanning
    constexpr std::size_t N = sizeof...(Args);
    auto args_tuple = std::forward_as_tuple(args...);

    std::size_t idx = 0;
    while (idx < s.size()) {
        if (s[idx] != '%') {
            std::cout.put(s[idx]);
            ++idx;
            continue;
        }
        if (idx + 1 >= s.size()) {
            throw format_error{"missing specifier after '%'"};
        }
        char c = s[idx + 1];
        if (c == '%') {
            std::cout.put('%');
            idx += 2;
            continue;
        }
        // Dispatch the i-th argument
        if (i >= N) throw format_error{"too many specifiers"};
        std::apply([&](auto const &...packed) {
            std::size_t j = 0;
            ((j == i ? (forward_arg(packed, c), void()) : void(), ++j), ...);
        }, args_tuple);
        ++i;
        idx += 2;
    }
    if (i != N) {
        throw format_error{"too few specifiers"};
    }
}

} // namespace sjtu
