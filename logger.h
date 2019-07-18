//
// Copyright [2019] [Comcast, Corp]
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <stdio.h>

enum class LogLevel
{
  Critical,
  Error,
  Warning,
  Info,
  Debug
};

enum class LogDestination
{
  Stdout,
  Syslog
};

class Logger
{
public:
  void log(LogLevel level, char const* file, int line, char const* msg, ...)
    __attribute__ ((format (printf, 5, 6)));

  inline bool isLevelEnabled(LogLevel level)
    { return level <= m_level; }

  void setLevel(LogLevel level);
  void setDestination(LogDestination dest);

  static char const* levelToString(LogLevel level);
  static LogLevel stringToLevel(char const* level);

  static Logger& logger();

private:
  Logger();

private:
  LogLevel       m_level;
  LogDestination m_dest;
};

#define XLOG(LEVEL, FORMAT, ...) \
    do { if (Logger::logger().isLevelEnabled(LEVEL)) { \
        Logger::logger().log(LEVEL, "", __LINE__ - 2, FORMAT, ##__VA_ARGS__); \
    } } while (0)

#define XLOG_DEBUG(FORMAT, ...) XLOG(LogLevel::Debug, FORMAT, ##__VA_ARGS__)
#define XLOG_INFO(FORMAT, ...) XLOG(LogLevel::Info, FORMAT, ##__VA_ARGS__)
#define XLOG_WARN(FORMAT, ...) XLOG(LogLevel::Warning, FORMAT, ##__VA_ARGS__)
#define XLOG_ERROR(FORMAT, ...) XLOG(LogLevel::Error, FORMAT, ##__VA_ARGS__)
#define XLOG_CRITICAL(FORMAT, ...) XLOG(LogLevel::Critical, FORMAT, ##__VA_ARGS__)
#define XLOG_FATAL(FORMAT, ...) XLOG(LogLevel::Critical, FORMAT, ##__VA_ARGS__)

#define XLOG_JSON(LEVEL, JSON) \
  do { if (Logger::logger().isLevelEnabled(LEVEL)) {\
    char* s = cJSON_Print(JSON); \
    Logger::logger().log(LEVEL, "", __LINE__ -2, "%s", s); \
    free(s); \
  } } while (0)

#endif
