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
#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdexcept>

#include <unistd.h>
#include <sys/syslog.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <mutex>

namespace
{
  std::mutex s_lock;

  struct LevelMapping
  {
    char const* s;
    LogLevel l;
  };

  LevelMapping mappings[] = 
  {
    { "CRIT",   LogLevel::Critical },
    { "ERROR",  LogLevel::Error },
    { "WARN",   LogLevel::Warning },
    { "INFO",   LogLevel::Info },
    { "DEBUG",  LogLevel::Debug }
  };

  int const kNumMappings = sizeof(mappings) / sizeof(LevelMapping);
  char const* kUnknownMapping = "UNKNOWN";

  int toSyslogPriority(LogLevel level)
  {
    int p = LOG_EMERG;
    switch (level)
    {
      case LogLevel::Critical:
        p = LOG_CRIT;
        break;
      case LogLevel::Error:
        p = LOG_ERR;
        break;
      case LogLevel::Warning:
        p = LOG_WARNING;
        break;
      case LogLevel::Info:
        p = LOG_INFO;
        break;
      case LogLevel::Debug:
        p = LOG_DEBUG;
        break;
    }
    return LOG_MAKEPRI(LOG_FAC(LOG_LOCAL4), LOG_PRI(p));
  }
}

void 
Logger::log(LogLevel level, char const* /*file*/, int /*line*/, char const* format, ...)
{

  va_list args;
  va_start(args, format);

  if (m_dest == LogDestination::Syslog)
  {
    int priority = toSyslogPriority(level);
    vsyslog(priority, format, args);
  }
  else
  {
    timeval tv;
    gettimeofday(&tv, 0);

    std::lock_guard<std::mutex> lock(s_lock);
    printf("%ld.%06ld (%5s) thr-%ld [%s] -- ", tv.tv_sec, tv.tv_usec, levelToString(level), syscall(__NR_gettid), "xconfigd");
    vprintf(format, args);
    printf("\n");
  }

  va_end(args);

  if (level == LogLevel::Critical)
  {
    this->log(LogLevel::Error, nullptr, __LINE__, "critical error, exiting");
    exit(1);
  }
}

char const*
Logger::levelToString(LogLevel level)
{
  for (int i = 0; i < kNumMappings; ++i)
  {
    if (mappings[i].l == level)
      return mappings[i].s;
  }
  return kUnknownMapping;
}

LogLevel
Logger::stringToLevel(char const* level)
{
  if (level)
  {
    for (int i = 0; i < kNumMappings; ++i)
    {
      if (strcasecmp(mappings[i].s, level) == 0)
        return mappings[i].l;
    }
  }
  throw std::runtime_error("invalid logging level " + std::string(level));
}

Logger&
Logger::logger()
{
  static Logger logger;
  return logger;
}

Logger::Logger()
  : m_level(LogLevel::Info)
  , m_dest(LogDestination::Stdout)
{

}

void Logger::setLevel(LogLevel level)
{
  m_level = level;
}

void Logger::setDestination(LogDestination dest)
{
  m_dest = dest;
}
