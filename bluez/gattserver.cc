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
#include "gattserver.h"
#include "beacon.h"
#include "../defs.h"
#include "../logger.h"
#include "../util.h"
#include "../jsonwrapper.h"
#include "../gattdata.h"

#include <exception>
#include <fstream>
#include <sstream>
#include <vector>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <cJSON.h>

// from bluez
extern "C" 
{
#include <src/shared/mainloop.h>
}

namespace
{
  char const     kRecordDelimiter         {30};
  uint16_t const kUuidDeviceInfoService   {0x180a};

  uint16_t const kUuidSystemId            {0x2a23};
  uint16_t const kUuidModelNumber         {0x2a24};
  uint16_t const kUuidSerialNumber        {0x2a25};
  uint16_t const kUuidFirmwareRevision    {0x2a26};
  uint16_t const kUuidHardwareRevision    {0x2a27};
  uint16_t const kUuidSoftwareRevision    {0x2a28};
  uint16_t const kUuidManufacturerName    {0x2a29};

  std::string const kUuidRpcService       {"503553ca-eb90-11e8-ac5b-bb7e434023e8"};
  std::string const kUuidRpcInbox         {"510c87c8-eb90-11e8-b3dc-17292c2ecc2d"};
  std::string const kUuidRpcEPoll         {"5140f882-eb90-11e8-a835-13d2bd922d3f"};

  //uint16_t const kUuidRdkDiagService      {0xFDB9};
  std::string const kUuidDeviceStatus     {"1f113f2c-cc01-4f03-9c5c-4b273ed631bb"};
  std::string const kUuidFwDownloadStatus {"915f96a6-3788-4271-a7ea-6820e98896b8"};
  std::string const kUuidWebPAStatus      {"9d5d3aae-51e3-4767-a055-59febd71de9d"};
  std::string const kUuidWiFiRadio1Status {"59a99d5a-3d2f-4265-af13-316c7c76b1f0"};
  std::string const kUuidWiFiRadio2Status {"9d6cf473-4fa6-4868-bf2b-c310f38df0c8"};
  std::string const kUuidRFStatus         {"91b9497e-634c-408a-9f77-8375b1461b8b"};

  void DIS_writeCallback(gatt_db_attribute* UNUSED_PARAM(attr), int err, void* UNUSED_PARAM(argp))
  {
    if (err)
    {
      XLOG_WARN("error writing to DIS service in GATT db. %d", err);
    }
  }

  void throw_errno(int err, char const* fmt, ...)
    __attribute__ ((format (printf, 2, 3)));

  void ATT_debugCallback(char const* str, void* UNUSED_PARAM(argp))
  {
    if (!str)
      XLOG_DEBUG("ATT: debug callback with no message");
    else
      XLOG_DEBUG("ATT: %s", str);
  }

  void GATT_debugCallback(char const* str, void* UNUSED_PARAM(argp))
  {
    if (!str)
      XLOG_DEBUG("GATT: debug callback with no message");
    else
      XLOG_DEBUG("GATT: %s", str);
  }

  void throw_errno(int e, char const* fmt, ...)
  {
    char buff[256] = {0};

    va_list args;
    va_start(args, fmt);
    vsnprintf(buff, sizeof(buff), fmt, args);
    buff[sizeof(buff) - 1] = '\0';
    va_end(args);

    char err[256] = {0};
    char* p = strerror_r(e, err, sizeof(err));

    std::stringstream out;
    if (strlen(buff) > 0)
    {
      out << buff;
      out << ". ";
    }
    if (p && strlen(p) > 0)
      out << p;

    std::string message(out.str());
    XLOG_ERROR("exception:%s", message.c_str());
    throw std::runtime_error(message);
  }

  void GattClient_onClientDisconnected(int err, void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onClientDisconnected(err);
  }

  void GattClient_onTimeout(int UNUSED_PARAM(fd), void* argp)
  {
    GattClient* clnt = reinterpret_cast<GattClient *>(argp);
    clnt->onTimeout();
  }
}

GattServer::GattServer()
  : m_listen_fd(-1)
{
  memset(&m_local_interface, 0, sizeof(m_local_interface));
}

GattServer::~GattServer()
{
  if (m_listen_fd != -1)
    close(m_listen_fd);
}

void
GattServer::init(cJSON const* listenerConfig)
{
  m_listen_fd = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  if (m_listen_fd < 0)
    throw_errno(errno, "failed to create bluetooth socket");

  bdaddr_t src_addr = {0}; // BDADDR_ANY

  sockaddr_l2 srcaddr;
  memset(&srcaddr, 0, sizeof(srcaddr));
  srcaddr.l2_family = AF_BLUETOOTH;
  srcaddr.l2_cid = htobs(4);
  srcaddr.l2_bdaddr_type = BDADDR_LE_PUBLIC;
  bacpy(&srcaddr.l2_bdaddr, &src_addr);

  int ret = bind(m_listen_fd, reinterpret_cast<sockaddr *>(&srcaddr), sizeof(srcaddr));
  if (ret < 0)
    throw_errno(errno, "failed to bind bluetooth socket");

  bt_security btsec = {0, 0};
  btsec.level = BT_SECURITY_LOW;
  ret = setsockopt(m_listen_fd, SOL_BLUETOOTH, BT_SECURITY, &btsec, sizeof(btsec));
  if (ret < 0)
    throw_errno(errno, "failed to set security on bluetooth socket");

  ret = listen(m_listen_fd, 2);
  if (ret < 0)
    throw_errno(errno, "failed to listen on bluetooth socket");

  startBeacon(listenerConfig);
}

std::shared_ptr<RpcConnectedClient>
GattServer::accept(DeviceInfoProvider const& deviceInfoProvider, RdkDiagProvider const& rdkDiagProvider)
{
  mainloop_init();

  sockaddr_l2 peer_addr;
  memset(&peer_addr, 0, sizeof(peer_addr));

  XLOG_INFO("waiting for incoming BLE connections");

  socklen_t n = sizeof(peer_addr);
  int soc = ::accept(m_listen_fd, reinterpret_cast<sockaddr *>(&peer_addr), &n);
  if (soc < 0)
    throw_errno(errno, "failed to accept incoming connection on bluetooth socket");

  char remote_address[64] = {0};
  ba2str(&peer_addr.l2_bdaddr, remote_address);
  XLOG_INFO("accepted remote connection from:%s", remote_address);

  auto clnt = std::shared_ptr<GattClient>(new GattClient(soc));
  clnt->init(deviceInfoProvider, rdkDiagProvider);
  return clnt;
}

void
GattClient::init(DeviceInfoProvider const& deviceInfoProvider, RdkDiagProvider const& rdkDiagProvider)
{
  m_att = bt_att_new(m_fd, 0);
  if (!m_att)
  {
    XLOG_ERROR("failed to create new att:%d", errno);
  }

  bt_att_set_close_on_unref(m_att, true);
  bt_att_register_disconnect(m_att, &GattClient_onClientDisconnected, this, nullptr);
  m_db = gatt_db_new();
  if (!m_db)
  {
    XLOG_ERROR("failed to create gatt database");
  }

  m_server = bt_gatt_server_new(m_db, m_att, m_mtu);
  if (!m_server)
  {
    XLOG_ERROR("failed to create gatt server");
  }

  if (true)
  {
    bt_att_set_debug(m_att, ATT_debugCallback, this, nullptr);
    bt_gatt_server_set_debug(m_server, GATT_debugCallback, this, nullptr);
  }

  m_timeout_id = mainloop_add_timeout(1000, &GattClient_onTimeout, this, nullptr);
  buildGattDatabase(deviceInfoProvider, rdkDiagProvider);
}

void
GattClient::buildGattDatabase(DeviceInfoProvider const& deviceInfoProvider, RdkDiagProvider const& rdkDiagProvider)
{
  buildDeviceInfoService(deviceInfoProvider);
  buildRdkDiagService(rdkDiagProvider);
}

void
GattClient::addGattCharacteristic(
  gatt_db_attribute* service,
  bt_uuid_t          uuid,
  std::string const& value)
{
  char buff[128];

  gatt_db_attribute* attr = gatt_db_service_add_characteristic(service, &uuid, BT_ATT_PERM_READ,
    BT_GATT_CHRC_PROP_READ, nullptr, nullptr, this);

  bt_uuid_to_string(&uuid, buff, sizeof(buff));
  if (!attr)
  {
    XLOG_CRITICAL("failed to create GATT characteristic %s", buff);
    return;
  }

  uint8_t const* p = reinterpret_cast<uint8_t const *>(value.c_str());

  XLOG_INFO("setting GATT attr:%s to %s", buff, value.c_str());

  // I'm not sure whether i like this or just having callbacks setup for reads
  gatt_db_attribute_write(attr, 0, p, value.length(), BT_ATT_OP_WRITE_REQ, nullptr,
      &DIS_writeCallback, nullptr);
}

void
GattClient::addGattCharacteristic(
  gatt_db_attribute* service,
  uint16_t           id,
  std::string const& value)
{
  bt_uuid_t uuid;
  bt_uuid16_create(&uuid, id);

  addGattCharacteristic(service, uuid, value);
}

void
GattClient::addGattCharacteristic(
  gatt_db_attribute* service,
  std::string const& id,
  std::string const& value)
{
  bt_uuid_t uuid;
  bt_string_to_uuid(&uuid, id.c_str());

  addGattCharacteristic(service, uuid, value);
}

void
GattClient::buildDeviceInfoService(DeviceInfoProvider const& deviceInfoProvider)
{
  bt_uuid_t uuid;
  bt_uuid16_create(&uuid, kUuidDeviceInfoService);

  gatt_db_attribute* service = gatt_db_add_service(m_db, &uuid, true, 30);

  XLOG_INFO("\nBuilding DeviceInfo Service");
  addGattCharacteristic(service, kUuidSystemId, deviceInfoProvider.GetSystemId());
  addGattCharacteristic(service, kUuidModelNumber, deviceInfoProvider.GetModelNumber());
  addGattCharacteristic(service, kUuidSerialNumber, deviceInfoProvider.GetSerialNumber());
  addGattCharacteristic(service, kUuidFirmwareRevision, deviceInfoProvider.GetFirmwareRevision());
  addGattCharacteristic(service, kUuidHardwareRevision, deviceInfoProvider.GetHardwareRevision());
  addGattCharacteristic(service, kUuidSoftwareRevision, deviceInfoProvider.GetSoftwareRevision());
  addGattCharacteristic(service, kUuidManufacturerName, deviceInfoProvider.GetManufacturerName());

  gatt_db_service_set_active(service, true);
}

void
GattClient::buildRdkDiagService(RdkDiagProvider const& rdkDiagProvider)
{
  bt_uuid_t uuid;
  bt_uuid16_create(&uuid, rdkDiagProvider.rdkDiagUuid);

  gatt_db_attribute* service = gatt_db_add_service(m_db, &uuid, true, 31);

  XLOG_INFO("\nBuilding RdkDiag Service");
  addGattCharacteristic(service, kUuidDeviceStatus, rdkDiagProvider.GetDeviceStatus());
  addGattCharacteristic(service, kUuidFwDownloadStatus, rdkDiagProvider.GetFirmwareDownloadStatus());
  addGattCharacteristic(service, kUuidWebPAStatus, rdkDiagProvider.GetWebPAStatus());
  addGattCharacteristic(service, kUuidWiFiRadio1Status, rdkDiagProvider.GetWiFiRadio1Status());
  addGattCharacteristic(service, kUuidWiFiRadio2Status, rdkDiagProvider.GetWiFiRadio2Status());
  addGattCharacteristic(service, kUuidRFStatus, rdkDiagProvider.GetRFStatus());

  gatt_db_service_set_active(service, true);
}

void
GattClient::onTimeout()
{
  uint32_t bytes_available = m_outgoing_queue.size();

  if (bytes_available > 0)
  {
    XLOG_INFO("notification of %u bytes available", bytes_available);

    bytes_available = htonl(bytes_available);

    //uint16_t handle = gatt_db_attribute_get_handle(m_blepoll);
    uint16_t handle = m_notify_handle;
    int ret = bt_gatt_server_send_notification(
      m_server,
      handle,
      reinterpret_cast<uint8_t *>(&bytes_available),
      sizeof(uint32_t));

    if (!ret)
    {
      XLOG_WARN("failed to send notification:%d with %u bytes pending",
        ret, bytes_available);
    }
  }

  mainloop_modify_timeout(m_timeout_id, 1000);
}

void
GattClient::run()
{
  m_mainloop_thread = std::this_thread::get_id();

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGTERM);

  mainloop_run();
}

GattClient::GattClient(int fd)
  : RpcConnectedClient()
  , m_fd(fd)
  , m_att(nullptr)
  , m_db(nullptr)
  , m_server(nullptr)
  , m_mtu(16)
  , m_outgoing_queue(kRecordDelimiter)
  , m_incoming_buff()
  , m_data_channel(nullptr)
  , m_blepoll(nullptr)
  , m_service_change_enabled(false)
  , m_timeout_id(-1)
  , m_mainloop_thread()
  , m_data_handler(nullptr)
{
}

GattClient::~GattClient()
{
  if (m_fd != -1)
    close(m_fd);

  if (m_server)
    bt_gatt_server_unref(m_server);

  if (m_db)
    gatt_db_unref(m_db);
}

void
GattClient::enqueueForSend(char const* buff, int n)
{
  if (!buff)
  {
    XLOG_WARN("trying to enqueue null buffer");
    return;
  }

  if (n <= 0)
  {
    XLOG_WARN("invalid buffer length:%d", n);
    return;
  }

  m_outgoing_queue.put_line(buff, n);
}

void
GattClient::onClientDisconnected(int err)
{
  // TODO: we should stash the remote client as a member of the
  // GattClient so we can print out mac addres of client that
  // just disconnected
  XLOG_INFO("disconnect:%d", err);
  mainloop_quit();
}
