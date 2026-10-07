#include "core/Logger.h"

#include <iostream>

namespace cultulhu {

void Logger::log(LogLevel level, const std::string& msg) {
    const char* tag = "?";
    switch (level) {
        case LogLevel::Debug: tag = "DBG"; break;
        case LogLevel::Info:  tag = "INF"; break;
        case LogLevel::Warn:  tag = "WRN"; break;
        case LogLevel::Error:  tag = "ERR"; break;
    }
    std::cout << "[cult-ulhu][" << tag << "] " << msg << "\n";
}

} // namespace cultulhu
