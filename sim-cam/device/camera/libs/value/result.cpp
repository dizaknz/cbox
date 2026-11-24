#include "value/result.h"
#include <cstdarg>
#include <memory>

namespace camera {

Error make_error(Error::Code code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::va_list args_copy;
    va_copy(args_copy, args);

    // Estimate buffer size
    auto size = std::vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    if (size < 0) {
        return Error{code, "format error"};
    }

    std::string msg(static_cast<size_t>(size), '\0');
    std::vsnprintf(msg.data(), msg.size() + 1, fmt, args_copy);
    va_end(args_copy);

    return Error{code, std::move(msg)};
}

} // namespace camera
