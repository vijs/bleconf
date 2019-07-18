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
#ifndef __GATT_SERVER_H__
#define __GATT_SERVER_H__

#include <list>
#include <memory>
#include <thread>
#include <vector>
#include <sstream>

#include "memory_stream.h"
#include "../rpcserver.h"

extern "C" 
{
#include <lib/bluetooth.h>
#include <lib/l2cap.h>
#include <lib/uuid.h>
#include <src/shared/att.h>
#include <src/shared/gatt-db.h>
#include <src/shared/gatt-server.h>
}

#include <cJSON.h>

struct gatt_db_attribute;

class GattClient : public RpcConnectedClient
{
public:
  GattClient(int fd);
  virtual ~GattClient();

  virtual void init(DeviceInfoProvider const& deviceInfoProvider, RdkDiagProvider const& rdkDiagProvider) override;
  virtual void enqueueForSend(char const* buff, int n) override; 
  virtual void run() override;
  virtual void setDataHandler(RpcDataHandler const& handler) override
    { m_data_handler = handler; }

  void onTimeout();
  void onClientDisconnected(int err);

private:
  void buildGattDatabase(DeviceInfoProvider const& deviceInfoProvider, RdkDiagProvider const& rdkDiagProvider);
  void addGattCharacteristic(gatt_db_attribute* service, bt_uuid_t uuid, std::string const& value);
  void addGattCharacteristic(gatt_db_attribute* service, uint16_t id, std::string const& value);
  void addGattCharacteristic(gatt_db_attribute* service, std::string const& id, std::string const& value);
  void buildDeviceInfoService(DeviceInfoProvider const& deviceInfoProvider);
  void buildRdkDiagService(RdkDiagProvider const& rdkDiagProvider);

private:
  int                 m_fd;
  bt_att*             m_att;
  gatt_db*            m_db;
  bt_gatt_server*     m_server;
  uint16_t            m_mtu;
  memory_stream       m_outgoing_queue;
  std::vector<char>   m_incoming_buff;
  gatt_db_attribute*  m_data_channel;
  gatt_db_attribute*  m_blepoll;
  uint16_t            m_notify_handle;
  bool                m_service_change_enabled;
  int                 m_timeout_id;
  std::thread::id     m_mainloop_thread;
  RpcDataHandler      m_data_handler;
};

class GattServer : public RpcListener
{
public:
  GattServer();
  virtual ~GattServer();

  virtual void init(cJSON const* conf) override;
  virtual std::shared_ptr<RpcConnectedClient>
    accept(DeviceInfoProvider const& deviceInfoProvider, RdkDiagProvider const& rdkDiagProvider) override;

private:
  int             m_listen_fd;
  bdaddr_t        m_local_interface;
};

#endif
