#pragma once
#include <format>

constexpr const char* ESC_CODE_BLACK   = "\x1b[30m";
constexpr const char* ESC_CODE_RED     = "\x1b[31m";
constexpr const char* ESC_CODE_GREEN   = "\x1b[32m";
constexpr const char* ESC_CODE_YELLOW  = "\x1b[33m";
constexpr const char* ESC_CODE_BLUE    = "\x1b[34m";
constexpr const char* ESC_CODE_MAGENTA = "\x1b[35m";
constexpr const char* ESC_CODE_CYAN    = "\x1b[36m";
constexpr const char* ESC_CODE_WHITE   = "\x1b[37m";
constexpr const char* ESC_CODE_RESET   = "\x1b[0m";

template <class... Args>
inline void LogInfo(std::format_string<Args...> fmt, Args&&... args) {
    std::cout 
        << ESC_CODE_GREEN 
        << std::format(fmt, std::forward<Args>(args)...) 
        << ESC_CODE_RESET
        << std::endl;
}

template <class... Args>
inline void LogInfo(std::wformat_string<Args...> fmt, Args&&... args) {
    std::wcout 
        << ESC_CODE_GREEN
        << std::format(fmt, std::forward<Args>(args)...) 
        << ESC_CODE_RESET
        << std::endl;
}

template <class... Args>
inline void LogWarning(std::format_string<Args...> fmt, Args&&... args) {
    std::cout
        << ESC_CODE_YELLOW
        << std::format(fmt, std::forward<Args>(args)...)
        << ESC_CODE_RESET
        << std::endl;
}

template <class... Args>
inline void LogWarning(std::wformat_string<Args...> fmt, Args&&... args) {
    std::wcout
        << ESC_CODE_YELLOW
        << std::format(fmt, std::forward<Args>(args)...)
        << ESC_CODE_RESET
        << std::endl;
}

template <class... Args>
inline void LogError(std::format_string<Args...> fmt, Args&&... args) {
    std::cerr 
        << ESC_CODE_RED 
        << std::format(fmt, std::forward<Args>(args)...) 
        << ESC_CODE_RESET 
        << std::endl;
}

template <class... Args>
inline void LogError(std::wformat_string<Args...> fmt, Args&&... args) {
    std::wcerr 
        << ESC_CODE_RED 
        << std::format(fmt, std::forward<Args>(args)...) 
        << ESC_CODE_RESET 
        << std::endl;
}

inline void LogFailedAssert(const char* expression, const char* file, unsigned line) {
    LogError("ASSERT FAILED:\n\tExpression: {}\n\tFile: {}\n\tLine: {}\n",
        expression, file, line);
}
template<class... Args>
inline void LogFailedAssertMsg(const char* expression, const char* file, unsigned line, std::format_string<Args...> fmt, Args&&...args) {
    const std::string msg = std::format(fmt, std::forward<Args>(args)...);
    LogError("ASSERT FAILED:\n\tExpression: {}\n\tFile: {}\n\tLine: {}\n\tMessage: {}\n",
        expression, file, line, msg);
}
template<class... Args>
inline void LogBreak(const char* file, unsigned line, std::format_string<Args...> fmt, Args&&...args) {
    const std::string msg = std::format(fmt, std::forward<Args>(args)...);
    LogError("BREAK:\n\tFile: {}\n\tLine: {}\n\tMessage: {}\n", file, line, msg);
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