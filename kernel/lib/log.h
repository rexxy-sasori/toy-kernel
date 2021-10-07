#pragma once

#include <stdint.h>

#include "../arch/asm.h"
#include "../ds/lock.h"

enum log_level_t : uint8_t {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL,
};

enum format_align_t : uint8_t {
    FMT_ALIGN_NONE,
    FMT_ALIGN_LEFT,
    FMT_ALIGN_CENTER,
    FMT_ALIGN_RIGHT,
};

enum format_sign_t : uint8_t {
    FMT_SIGN_NONE,
    FMT_SIGN_PLUS,
    FMT_SIGN_MINUS,
    FMT_SIGN_SPACE,
};

enum format_repr_t : uint8_t {
    FMT_REPR_NONE,
    FMT_REPR_BINARY,
    FMT_REPR_OCTAL,
    FMT_REPR_DECIMAL,
    FMT_REPR_HEX,
    FMT_REPR_HEX_UPPER,
};

class format_options_t {
    const char *parse_align(const char *options);
    const char *parse_sign(const char *options);
    const char *parse_width(const char *options);
    const char *parse_repr(const char *options);

    uint8_t m_fill_char;
    uint8_t m_alignment;
    uint8_t m_sign;
    uint8_t m_width;
    uint8_t m_repr;

    bool m_alternate;
    bool m_zero_pad;

public:
    format_options_t() = default;

    const char *parse(const char *options);

    uint8_t get_fill_char() const { return m_fill_char; }
    uint8_t get_alignment() const { return m_alignment; }
    uint8_t get_sign() const { return m_sign; }
    uint8_t get_width() const { return m_width; }
    uint8_t get_repr() const { return m_repr; }

    bool get_alternate() const { return m_alternate; }
    bool get_zero_pad() const { return m_zero_pad; }

    void set_fill_char(uint8_t fill_char) { m_fill_char = fill_char; }
    void set_alignment(uint8_t alignment) { m_alignment = alignment; }
    void set_sign(uint8_t sign) { m_sign = sign; }
    void set_width(uint8_t width) { m_width = width; }
    void set_repr(uint8_t repr) { m_repr = repr; }
    void set_alternate(bool alternate) { m_alternate = alternate; }
    void set_zero_pad(bool zero_pad) { m_zero_pad = zero_pad; }
};

template <typename T>
class formatter_t {
public:
    static void format(const T &value, const format_options_t &options);
};

template <typename T>
requires(__is_pointer(T)) class formatter_t<T> {
public:
    static void format(T value, const format_options_t &options) { formatter_t<uint64_t>::format((uint64_t) value, options); }
};

template <>
class formatter_t<const char *> {
public:
    static void format(const char *value, const format_options_t &options);
};

template <>
class formatter_t<char *> {
public:
    static void format(char *value, const format_options_t &options);
};

template <uint64_t N>
class formatter_t<char[N]> {
public:
    static void format(const char *value, const format_options_t &options) { formatter_t<const char *>::format(value, options); }
};

template <uint64_t N>
class formatter_t<const char (&)[N]> {
    static void format(const char *value, const format_options_t &options) { formatter_t<const char *>::format(value, options); }
};

namespace detail {

inline lock_t log_lock;

template <typename T>
void format_arg(const T &value, const format_options_t &options) {
    formatter_t<T>::format(value, options);
}

template <typename... Args>
void print_format(const char *format, Args &&...args) {
    const auto handle_arg = [&](auto &arg) {
        while (*format) {
            if (*format != '{') {
                format_arg(*format++, {});

                continue;
            }

            format++;

            if (*format == '{') {
                format_arg('{', {});

                continue;
            }

            format_options_t options;

            format = options.parse(format);

            format_arg(arg, options);

            return;
        }
    };

    (handle_arg(args), ...);

    format_arg(format, {});
}

template <typename... Args>
void print_format_log_unlocked(log_level_t level, const char *file, int line, const char *format, Args &&...args) {
    print_format("\e[1m[{}]\e[m [{}:{}] ", level, file, line);
    print_format(format, args...);

    format_arg('\n', {});
}

template <typename... Args>
void print_format_log(log_level_t level, const char *file, int line, const char *format, Args &&...args) {
    lock_guard_t lock(log_lock);

    print_format_log_unlocked(level, file, line, format, args...);
}

} // namespace detail

#define log_debug_unlocked(...) detail::print_format_log_unlocked(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info_unlocked(...) detail::print_format_log_unlocked(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn_unlocked(...) detail::print_format_log_unlocked(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error_unlocked(...) detail::print_format_log_unlocked(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal_unlocked(...)                                                                                                            \
    do {                                                                                                                                   \
        detail::print_format_log_unlocked(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__);                                               \
        arch::halt();                                                                                                                      \
    } while (true)

#define log_debug(...) detail::print_format_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...) detail::print_format_log(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...) detail::print_format_log(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) detail::print_format_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define log_fatal(...)                                                                                                                     \
    do {                                                                                                                                   \
        detail::print_format_log(LOG_LEVEL_FATAL, __FILE__, __LINE__, __VA_ARGS__);                                                        \
        arch::halt();                                                                                                                      \
    } while (true)

#define assert_msg(cond, ...)                                                                                                              \
    do {                                                                                                                                   \
        if (!(cond))                                                                                                                       \
            log_fatal(__VA_ARGS__);                                                                                                        \
    } while (true)

#define assert(cond) assert_msg(cond, "Assertion failed: {}", #cond)
