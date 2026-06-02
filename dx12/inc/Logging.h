#pragma once
#include <fmt/format.h>
#include <fmt/printf.h>
#include <fmt/xchar.h>


template<class... Args>
inline void LogError(std::string_view msg, Args&&... args) {
    std::cerr << fmt::format(fmt::runtime(msg), std::forward<Args>(args)...) << std::endl;
}

inline void LogFailedAssert(const char* expression, const char* file, unsigned line) {
    std::cerr << fmt::format(fmt::runtime("ASSERT FAILED:\n\tExpression: {}\n\tFile: {}\n\tLine: {}\n"),
        expression, file, line) << std::endl;
}
template<class... Args>
inline void LogFailedAssertMsg(const char* expression, const char* file, unsigned line, const char* message, Args&&...args) {
    const std::string msg = fmt::format(fmt::runtime(message), std::forward<Args>(args)...);
    std::cerr << fmt::format(fmt::runtime("ASSERT FAILED:\n\tExpression: {}\n\tFile: {}\n\tLine: {}\n\tMessage: {}\n"),
        expression, file, line, msg) << std::endl;
}
template<class... Args>
inline void LogBreak(const char* file, unsigned line, const char* message, Args&&...args) {
    const std::string msg = fmt::format(fmt::runtime(message), std::forward<Args>(args)...);
    std::cerr << fmt::format(fmt::runtime("BREAK:\n\tFile: {}\n\tLine: {}\n\tMessage: {}\n"), file, line, msg) << std::endl;
}

#ifdef _DEBUG

/** Assert if the expression evaluates to true. If not, breaks the application and logs the error.
 * \param expression The expression to evaluate.
 */
#define ASSERT(expression) if ((!(expression))) { \
		LogFailedAssert((#expression), (__FILE__), (unsigned)(__LINE__)); \
        __debugbreak(); }

 /** Assert if the expression evaluates to true.
  * If not, breaks the application and logs the error and the user supplied message.
  * \param expression The expression to evaluate.
  * \param message A const char array in the fmt format that describes what caused the break.
  * \param ... Arguments to be used in the message. These must be convertible to fmt::format_args.
  */
#define ASSERT_MSG(expression, message, ...) if ((!(expression))) { \
		LogFailedAssertMsg((#expression), (__FILE__), (unsigned)(__LINE__), (message), __VA_ARGS__); \
        __debugbreak(); }

  /** Breaks the application and logs the user supplied message.
   * \param message A const char array in the fmt format that describes what caused the break.
   * \param ... Arguments to be used in the message. These must be convertible to fmt::format_args.
   */
#define BREAK(message, ...) LogBreak((__FILE__), (unsigned)(__LINE__), (message), __VA_ARGS__); \
		__debugbreak()

#else

#define ASSERT(expression) ((void)0)
#define ASSERT_MSG(expression, message, ...) ((void)0)
#define BREAK(message, ...) ((void)0)

#endif