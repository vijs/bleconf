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
#include "jsonwrapper.h"
#include "logger.h"
#include "defs.h"

#include <iomanip>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

cJSON*
JsonWrapper::makeError(int code, char const* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  int n = vsnprintf(0, 0, fmt, ap);
  va_end(ap);

  char* buff = new char[n + 1 ];
  buff[n] = '\0';

  va_start(ap, fmt);
  n = vsnprintf(buff, n + 1, fmt, ap);

  cJSON* result = cJSON_CreateObject();
  cJSON_AddItemToObject(result, "code", cJSON_CreateNumber(code));
  cJSON_AddItemToObject(result, "message", cJSON_CreateString(buff));

  delete [] buff;

  return result;
}


cJSON const*
JsonWrapper::search(cJSON const* json, char const* name, bool required)
{
  if (!json)
  {
    std::stringstream buff;
    buff << "null json object when fetching field";
    if (name)
      buff << ":" << name;
    throw std::runtime_error(buff.str());
  }

  if (!name || strlen(name) == 0)
  {
    std::stringstream buff;
    buff << "null name when fetching json field";
    throw std::runtime_error(buff.str());
  }

  cJSON const* item = nullptr;

  if (name[0] == '/')
  {
    char* str = strdup(name + 1);
    char* saveptr = nullptr;
    char* token = nullptr;

    item = json;
    while ((token = strtok_r(str, "/", &saveptr)) != nullptr)
    {
      if (token == nullptr)
        break;

      item = cJSON_GetObjectItem(item, token);
      if (!item)
        break;

      str = nullptr;
    }

    free(str);
  }
  else
  {
    item = cJSON_GetObjectItem(json, name);
  }

  if (!item && required)
  {
    char* s = cJSON_Print(json);
    XLOG_INFO("missing %s from the following json", name);
    XLOG_INFO("%s", s);
    free(s);

    std::stringstream buff;
    buff << "missing field ";
    buff << name;
    buff << " from object";
    throw std::runtime_error(buff.str());
  }

  return item;
}

int
JsonWrapper::getInt(cJSON const* req, char const* name, bool required, int defaultValue)
{
  int n = defaultValue;
  cJSON const* item = JsonWrapper::search(req, name, required);
  if (item)
    n = item->valueint;
  return n;
}

char const*
JsonWrapper::getString(cJSON const* req, char const* name, bool required, char const* defaultValue)
{
  char const* s = defaultValue;

  cJSON const* item = JsonWrapper::search(req, name, required);
  if (item)
    s = item->valuestring;

  if (!s || (strcmp(s, "<null>") == 0))
    s = NULL;

  return s;
}

cJSON*
JsonWrapper::wrapResponse(int code, cJSON* res, int reqId)
{
  cJSON* envelope = cJSON_CreateObject();
  cJSON_AddStringToObject(envelope, "jsonrpc", kJsonRpcVersion);
  if (reqId != -1)
    cJSON_AddNumberToObject(envelope, "id", reqId);
  if (code == 0)
    cJSON_AddItemToObject(envelope, "result", res);
  else
    cJSON_AddItemToObject(envelope, "error", res);
  return envelope;
}

cJSON*
JsonWrapper::notImplemented(char const* methodName)
{
  return JsonWrapper::makeError(ENOENT, "method %s not implemented", methodName);
}

cJSON*
JsonWrapper::fromFile(char const* fname)
{
  cJSON* json = nullptr;

  if (!fname || strlen(fname) == 0)
    return nullptr;

  std::ifstream in;
  in.exceptions(std::ios::failbit);
  in.open(fname);

  std::string buff((std::istreambuf_iterator<char>(in)), (std::istreambuf_iterator<char>()));
  json = cJSON_Parse(buff.c_str());
  if (!json)
  {
    XLOG_WARN("failed to parse json file %s", fname);
    // TODO: report parsing error
  }

  return json;
}
