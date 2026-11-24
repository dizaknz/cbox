#pragma once
// result.h — expected-style Result<T, E> (no exceptions)
// All fallible functions return Result<T, Error> and are [[nodiscard]].

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

namespace camera {

// ── Error type ────────────────────────────────────────────────────

class Error {
  public:
    enum class Code {
        Ok,
        NotFound,
        AlreadyExists,
        InvalidArgument,
        IoError,
        Timeout,
        BufferTooSmall,
        SchemaError,
        Internal,
    };

    [[nodiscard]] Code code() const noexcept { return code_; }
    [[nodiscard]] const char *message() const noexcept { return msg_.c_str(); }

    std::string what() const { return std::string{message()}; }

  private:
    Code code_ = Code::Ok;
    std::string msg_;

    explicit Error(Code code, std::string msg) : code_(code), msg_(std::move(msg)) {}
    template <typename, typename> friend class ResultT;
    friend Error make_error(Error::Code code, const char *fmt, ...);
};

// ── Error factory ─────────────────────────────────────────────────
// Static method for formatted error messages.

Error make_error(Error::Code code, const char *fmt, ...);

// ── Result<T, E> ──────────────────────────────────────────────────

template <typename T, typename E = Error> class ResultT {
  public:
    using value_type = T;
    using error_type = E;

    // Construct success
    static ResultT ok(T value) noexcept {
        ResultT r;
        r.is_ok_ = true;
        new (&r.value_storage_) T(std::move(value));
        return r;
    }

    // Construct error
    static ResultT err(E error) noexcept {
        ResultT r;
        r.is_ok_ = false;
        new (&r.error_storage_) E(std::move(error));
        return r;
    }

    [[nodiscard]] bool ok() const noexcept { return is_ok_; }
    [[nodiscard]] bool err() const noexcept { return !is_ok_; }

    // Create a formatted error (convenience for Result<T>)
    static Error make_error(Error::Code code, const char *fmt, ...) { return make_error(code, fmt); }

    [[nodiscard]] const T &value() const & {
        if (!is_ok_)
            throw std::runtime_error("ResultT: accessed error as value");
        return *reinterpret_cast<const T *>(&value_storage_);
    }
    [[nodiscard]] T &&value() && {
        if (!is_ok_)
            throw std::runtime_error("ResultT: accessed error as value");
        return std::move(*reinterpret_cast<T *>(&value_storage_));
    }

    [[nodiscard]] const E &error() const & {
        if (is_ok_)
            throw std::runtime_error("ResultT: accessed value as error");
        return *reinterpret_cast<const E *>(&error_storage_);
    }
    [[nodiscard]] E &&error() && {
        if (is_ok_)
            throw std::runtime_error("ResultT: accessed value as error");
        return std::move(*reinterpret_cast<E *>(&error_storage_));
    }

    // Unwrap or default
    [[nodiscard]] const T &unwrap_or(const T &default_val) const & {
        return is_ok_ ? *reinterpret_cast<const T *>(&value_storage_) : default_val;
    }

    ~ResultT() {
        if (is_ok_) {
            reinterpret_cast<T *>(&value_storage_)->~T();
        } else {
            reinterpret_cast<E *>(&error_storage_)->~E();
        }
    }

    // Non-copyable due to variant storage, but movable
    ResultT(ResultT &&other) noexcept : is_ok_(other.is_ok_) {
        if (is_ok_) {
            new (&value_storage_) T(std::move(*reinterpret_cast<T *>(&other.value_storage_)));
        } else {
            new (&error_storage_) E(std::move(*reinterpret_cast<E *>(&other.error_storage_)));
        }
    }

    ResultT &operator=(ResultT &&other) noexcept {
        if (this != &other) {
            this->~ResultT();
            is_ok_ = other.is_ok_;
            if (is_ok_) {
                new (&value_storage_) T(std::move(*reinterpret_cast<T *>(&other.value_storage_)));
            } else {
                new (&error_storage_) E(std::move(*reinterpret_cast<E *>(&other.error_storage_)));
            }
        }
        return *this;
    }

    ResultT(const ResultT &) = delete;
    ResultT &operator=(const ResultT &) = delete;

  private:
    alignas(T) unsigned char value_storage_[sizeof(T)];
    alignas(E) unsigned char error_storage_[sizeof(E)];
    bool is_ok_ = false;

    ResultT() = default;
};

// ── ResultT<void, E> specialization ───────────────────────────────
// For functions that return void on success but Error on failure.
// Uses `ok()` for the factory method and `is_ok()`/`is_err()` for checks.

template <typename E> class ResultT<void, E> {
  public:
    using error_type = E;

    // Construct success (no value)
    static ResultT ok() noexcept {
        ResultT r;
        r.is_ok_ = true;
        return r;
    }

    // Construct error
    static ResultT err(E error) noexcept {
        ResultT r;
        r.is_ok_ = false;
        new (&r.error_storage_) E(std::move(error));
        return r;
    }

    // Create a formatted error (convenience for Result<void>)
    static Error make_error(Error::Code code, const char *fmt, ...) { return make_error(code, fmt); }

    [[nodiscard]] bool is_ok() const noexcept { return is_ok_; }
    [[nodiscard]] bool is_err() const noexcept { return !is_ok_; }

    [[nodiscard]] const E &error() const & {
        if (is_ok_)
            throw std::runtime_error("ResultT<void>: accessed value as error");
        return *reinterpret_cast<const E *>(&error_storage_);
    }
    [[nodiscard]] E &&error() && {
        if (is_ok_)
            throw std::runtime_error("ResultT<void>: accessed value as error");
        return std::move(*reinterpret_cast<E *>(&error_storage_));
    }

    ~ResultT() {
        if (!is_ok_) {
            reinterpret_cast<E *>(&error_storage_)->~E();
        }
    }

    // Non-copyable, but movable
    ResultT(ResultT &&other) noexcept : is_ok_(other.is_ok_) {
        if (!is_ok_) {
            new (&error_storage_) E(std::move(*reinterpret_cast<E *>(&other.error_storage_)));
        }
    }

    ResultT &operator=(ResultT &&other) noexcept {
        if (this != &other) {
            this->~ResultT();
            is_ok_ = other.is_ok_;
            if (!is_ok_) {
                new (&error_storage_) E(std::move(*reinterpret_cast<E *>(&other.error_storage_)));
            }
        }
        return *this;
    }

    ResultT(const ResultT &) = delete;
    ResultT &operator=(const ResultT &) = delete;

  private:
    alignas(E) unsigned char error_storage_[sizeof(E)];
    bool is_ok_ = false;

    ResultT() = default;
};

// ── Common aliases ────────────────────────────────────────────────

using Result = ResultT<void>;  // void-returning fallible op (ok() factory, is_ok()/is_err() checks)
using Expected = ResultT<int>; // common alias for int-returning fallible ops

// Deduction guide (removed - causes issues with MSVC)
// template <typename T> ResultT(T) -> ResultT<T>;

} // namespace camera
