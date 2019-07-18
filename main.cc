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
#include "defs.h"
#include "logger.h"
#include "rpcserver.h"
#include "jsonwrapper.h"
#include "util.h"
#include "gattdata.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cJSON.h>

void
printHelp()
{
  printf("\n");
  printf("bleconfd [args]\n");
  printf("\t-c  --config <file> Confuguration file\n");
  printf("\t-d  --debug         Enable debug logging\n");
  printf("\t-t  --test   <file> Read json file, exec, and exit\n");
  printf("\t-h  --help          Print this help and exit\n");
  exit(0);
}

int main(int argc, char* argv[])
{
  std::string configFile;
#ifdef PLATFORM_RASPBERRYPI
  configFile = "pi/config.json";
#endif
  Logger::logger().setLevel(LogLevel::Info);

  while (true)
  {
    static struct option longOptions[] = 
    {
      { "config", required_argument, 0, 'c' },
      { "debug",  no_argument, 0, 'd' },
      { "help",   no_argument, 0, 'h' },
      { 0, 0, 0, 0 }
    };

    int optionIndex = 0;
    int c = getopt_long(argc, argv, "c:dt:", longOptions, &optionIndex);
    if (c == -1)
      break;

    switch (c)
    {
      case 'c':
        configFile = optarg;
        break;
      case 'd':
        Logger::logger().setLevel(LogLevel::Debug);
        break;
      case 'h':
        printHelp();
        break;
      default:
        break;
    }
  }

  XLOG_INFO("loading configuration from file %s", configFile.c_str());
  cJSON* config = JsonWrapper::fromFile(configFile.c_str());
  RpcServer server(configFile, config);
  XLOG_INFO("rpc server intialized");

  cJSON const* listenerConfig = cJSON_GetObjectItem(config, "listener");
  std::shared_ptr<GattData> gattDataProvider(GattData::create());
  GattServiceDataProvider dataProvider = gattDataProvider->getDataProvider(listenerConfig);

  while (true)
  {
    try
    {
      std::shared_ptr<RpcListener> listener(RpcListener::create());
      listener->init(listenerConfig);

      // blocks here until remote client makes BT connection
      std::shared_ptr<RpcConnectedClient> client = listener->accept(dataProvider.deviceInfoProvider, dataProvider.rdkDiagProvider);
      //client->setDataHandler(std::bind(&RpcServer::onIncomingMessage,
      //      &server, std::placeholders::_1, std::placeholders::_2));
      server.setClient(client);
      server.run();
    }
    catch (std::runtime_error const& err)
    {
      XLOG_ERROR("unhandled exception:%s", err.what());
      return -1;
    }
  }

  return 0;
}