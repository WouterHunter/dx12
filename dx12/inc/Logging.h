#pragma once
#include <format>

template<typename... Args>
void print(std::FILE* f, std::format_string<Args...> fmt, Args&&... args) {
	std::fputs(std::format(fmt, std::forward<Args>(args)...).c_str(), f);
}
template<typename... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
	print(stdout, fmt, std::forward<Args>(args)...);
}
template<typename... Args>
void wprint(std::FILE* f, std::wformat_string<Args...> fmt, Args&&... args) {
	std::fputws(std::format(fmt, std::forward<Args>(args)...).c_str(), f);
}
template<typename... Args>
void wprint(std::wformat_string<Args...> fmt, Args&&... args) {
	wprint(stdout, fmt, std::forward<Args>(args)...);
}

enum class EscCode { Black, Red, Green, Yellow, Blue, Magenta, Cyan, White, Reset };

template<typename Char = char>
constexpr std::basic_string_view<Char> ToString(EscCode escCode) {
	if constexpr (std::is_same_v<Char, wchar_t>) {
		switch (escCode) {
			case EscCode::Black:   return L"\x1b[30m";
			case EscCode::Red:     return L"\x1b[31m";
			case EscCode::Green:   return L"\x1b[32m";
			case EscCode::Yellow:  return L"\x1b[33m";
			case EscCode::Blue:    return L"\x1b[34m";
			case EscCode::Magenta: return L"\x1b[35m";
			case EscCode::Cyan:    return L"\x1b[36m";
			case EscCode::White:   return L"\x1b[37m";
			case EscCode::Reset:   return L"\x1b[0m";
			default:               return L"";
		}
	} else {
		switch (escCode) {
			case EscCode::Black:   return "\x1b[30m";
			case EscCode::Red:     return "\x1b[31m";
			case EscCode::Green:   return "\x1b[32m";
			case EscCode::Yellow:  return "\x1b[33m";
			case EscCode::Blue:    return "\x1b[34m";
			case EscCode::Magenta: return "\x1b[35m";
			case EscCode::Cyan:    return "\x1b[36m";
			case EscCode::White:   return "\x1b[37m";
			case EscCode::Reset:   return "\x1b[0m";
			default:               return "";
		}
	}
}

template<typename Char>
inline std::basic_string<Char> FormatTitleString(const Char* category, EscCode escCode) {
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::zoned_time{
		std::chrono::current_zone(),
		std::chrono::floor<std::chrono::milliseconds>(now)
	};
	if constexpr (std::is_same_v<Char, wchar_t>)
		return std::format(L"{}{: <7} [{:%H:%M:%S}]:{}",
			ToString<wchar_t>(escCode), category, time,
			ToString<wchar_t>(EscCode::Reset));
	else
		return std::format("{}{: <7} [{:%H:%M:%S}]:{}",
			ToString(escCode), category, time,
			ToString(EscCode::Reset));
}

template <class... Args>
inline void LogInfo(std::format_string<Args...> fmt, Args&&... args) {
	print("{} {}\n",
		FormatTitleString<char>("Info", EscCode::Green),
		std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
inline void LogInfo(std::wformat_string<Args...> fmt, Args&&... args) {
	wprint(L"{} {}\n",
		FormatTitleString<wchar_t>(L"Info", EscCode::Green),
		std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
inline void LogWarning(std::format_string<Args...> fmt, Args&&... args) {
	print("{} {}\n",
		FormatTitleString<char>("Warning", EscCode::Yellow),
		std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
inline void LogWarning(std::wformat_string<Args...> fmt, Args&&... args) {
	wprint(L"{} {}\n",
		FormatTitleString<wchar_t>(L"Warning", EscCode::Yellow),
		std::format(fmt, std::forward<Args>(args)...));
}

template <class... Args>
inline void LogError(std::format_string<Args...> fmt, Args&&... args) {
	print("{} {}\n",
		FormatTitleString<char>("Error", EscCode::Red),
		std::format(fmt, std::forward<Args>(args)...));
}
template <class... Args>
inline void LogError(std::wformat_string<Args...> fmt, Args&&... args) {
	wprint(L"{} {}\n",
		FormatTitleString<wchar_t>(L"Error", EscCode::Red),
		std::format(fmt, std::forward<Args>(args)...));
}

#define LOG_INFO(fmt, ...) LogInfo(  \
		"{}({}): {}",                \
		(__FILE__), (u32)(__LINE__), \
		std::format((fmt) __VA_OPT__(,) __VA_ARGS__));
#define WLOG_INFO(fmt, ...) LogInfo(  \
		L"{}({}): {}",                \
		(__FILEW__), (u32)(__LINE__), \
		std::format((fmt) __VA_OPT__(,) __VA_ARGS__));

#define LOG_WARN(fmt, ...) LogWarning(  \
		"{}({}): {}",                   \
		(__FILE__), (u32)(__LINE__),    \
		std::format((fmt) __VA_OPT__(,) __VA_ARGS__));
#define WLOG_WARN(fmt, ...) LogWarning(  \
		L"{}({}): {}",                   \
		(__FILEW__), (u32)(__LINE__),    \
		std::format((fmt) __VA_OPT__(,) __VA_ARGS__));

#define LOG_ERROR(fmt, ...) LogError(  \
		"{}({}): {}",                  \
		(__FILE__), (u32)(__LINE__),   \
		std::format((fmt) __VA_OPT__(,) __VA_ARGS__));
#define WLOG_ERROR(fmt, ...) LogError(  \
		L"{}({}): {}",                  \
		(__FILEW__), (u32)(__LINE__),   \
		std::format((fmt) __VA_OPT__(,) __VA_ARGS__));

inline void LogFailedAssert(const char* expression, const char* file, unsigned line) {
    LogError("ASSERT FAILED:\n\tExpression: {}\n\tFile: {}\n\tLine: {}\n",
        expression, file, line);
}
template<class... Args>
inline void LogFailedAssert(const char* expression, const char* file, unsigned line, std::format_string<Args...> fmt, Args&&... args) {
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

/** Assert if the expression evaluates to true.
* If not, breaks the application and logs the error and an optional message.
* \param expression The expression to evaluate.
* \param message A const char array in the fmt format that describes what caused the break.
* \param ... Arguments to be used in the message. These must be convertible to fmt::format_args.
*/
#define ASSERT(expression, ...) if ((!(expression))) { \
    LogFailedAssert((#expression), (__FILE__), (unsigned)(__LINE__) __VA_OPT__(,) __VA_ARGS__); \
    __debugbreak(); }

/** Breaks the application and logs the message.
 * \param message A const char array in the fmt format that describes what caused the break.
 * \param ... Arguments to be used in the message. These must be convertible to fmt::format_args.
 */
#define BREAK(message, ...) LogBreak((__FILE__), (unsigned)(__LINE__), (message) __VA_OPT__(,) __VA_ARGS__); \
    __debugbreak()

#else

#define ASSERT(expression, ...) ((void)0)
#define BREAK(message, ...) ((void)0)

#endif