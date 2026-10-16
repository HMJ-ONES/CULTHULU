#pragma once

#include <string>

namespace cultulhu {

enum class LogLevel { Debug, Info, Warn, Error };

// Tiny static logger; keeps systems decoupled from any engine log facility.
class Logger {
public:
    static void log(LogLevel level, const std::string& msg);
    static void debug(const std::string& msg) { log(LogLevel::Debug, msg); }
    static void info(const std::string& msg)  { log(LogLevel::Info, msg); }
    static void warn(const std::string& msg)  { log(LogLevel::Warn, msg); }
    static void error(const std::string& msg) { log(LogLevel::Error, msg); }
};

} // namespace cultulhu
