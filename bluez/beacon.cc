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
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <getopt.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <lib/bluetooth/bluetooth.h>
#include <lib/bluetooth/hci.h>
#include <lib/bluetooth/hci_lib.h>
#include <lib/hci.h>

#include "../logger.h"
#include "../util.h"
#include "../jsonwrapper.h"

#include "beacon.h"

using std::string;
using std::vector;



/**
 * Stop HCI device
 * @param ctl the device controller(fd)
 * @param hdev the device id
 */
void
cmdDown(int ctl, int hdev)
{
  if (ioctl(ctl, HCIDEVDOWN, hdev) < 0)
  {
    XLOG_FATAL("Can't down device hci%d: %s (%d)", hdev, strerror(errno), errno);
  }
}

/**
 * Start HCI device
 * @param ctl the device controller(fd)
 * @param hdev the device id
 */
void
cmdUp(int ctl, int hdev)
{
  if (ioctl(ctl, HCIDEVUP, hdev) < 0)
  {
    if (errno == EALREADY)
      return;
    XLOG_FATAL("Can't init device hci%d: %s (%d)", hdev, strerror(errno), errno);
  }
}

/**
 * send no lead v cmd to device
 * @param hdev the device id
 */
void
cmdNoleadv(int hdev)
{
  hci_request req;
  le_set_advertise_enable_cp advertise_cp;

  uint8_t status = 0;
  int dd = 0;
  int ret = 0;

  if (hdev < 0)
    hdev = hci_get_route(NULL);

  dd = hci_open_dev(hdev);
  if (dd < 0)
  {
    XLOG_FATAL("Could not open device (%d)", hdev);
  }

  memset(&advertise_cp, 0, sizeof(advertise_cp));
  advertise_cp.enable = 0x00;

  memset(&req, 0, sizeof(req));
  req.ogf = OGF_LE_CTL;
  req.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
  req.cparam = &advertise_cp;
  req.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
  req.rparam = &status;
  req.rlen = 1;

  ret = hci_send_req(dd, &req, 1000);

  hci_close_dev(dd);

  if (ret < 0)
  {
    XLOG_FATAL("Can't set advertise mode on hci%d: %s (%d)", hdev,
      strerror(errno), errno);
  }

  // TODO: what exactly is "status"
  if (status)
  {
    XLOG_INFO("Disabling LE advertise on hci%d returned error status:%d.",
      hdev, status);
  }
}

/**
 * send lead v to device
 * @param hdev  the device id
 */
void
cmdLeadv(int hdev)
{
  struct hci_request rq;
  le_set_advertise_enable_cp advertise_cp;
  le_set_advertising_parameters_cp adv_params_cp;
  uint8_t status;
  int dd, ret;

  if (hdev < 0)
    hdev = hci_get_route(NULL);

  dd = hci_open_dev(hdev);
  if (dd < 0)
  {
    XLOG_FATAL("Could not open device (%d)", hdev);
  }

  memset(&adv_params_cp, 0, sizeof(adv_params_cp));
  adv_params_cp.min_interval = htobs(0x0800);
  adv_params_cp.max_interval = htobs(0x0800);
  adv_params_cp.chan_map = 7;

  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  rq.cparam = &adv_params_cp;
  rq.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  ret = hci_send_req(dd, &rq, 1000);
  if (ret >= 0)
  {
    memset(&advertise_cp, 0, sizeof(advertise_cp));
    advertise_cp.enable = 0x01;

    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISE_ENABLE;
    rq.cparam = &advertise_cp;
    rq.clen = LE_SET_ADVERTISE_ENABLE_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;
    ret = hci_send_req(dd, &rq, 1000);
  }

  hci_close_dev(dd);

  if (ret < 0)
  {
    XLOG_FATAL("Can't set advertise mode on hci%d: %s (%d)", hdev,
      strerror(errno), errno);
  }

  if (status)
  {
    XLOG_WARN("Enabling LE advertise on hci%d returned error status:%d",
        hdev, status);
  }
}


/**
 * dump hex to console
 * @param width the hex width
 * @param buf the hex buffer
 * @param len the buffer length
 */
void
hex_dump(int width, unsigned char* buf, int len)
{
  char const* pref = "  ";
  register int i, n;
  for (i = 0, n = 1; i < len; i++, n++)
  {
    if (n == 1)
      printf("%s", pref);
    printf("%2.2X ", buf[i]);
    if (n == width)
    {
      printf("\n");
      n = 0;
    }
  }
  if (i && n != 1)
    printf("\n");
}


/**
 * update device name, not sure why it not worked as expected ???
 * keep it for now
 * @param hdev the device id
 * @param deviceName  the device name
 */
void
cmdName(int hdev, char const* deviceName)
{
  int dd = hci_open_dev(hdev);
  if (dd < 0)
  {
    XLOG_FATAL("Can't open device hci%d: %s (%d)", hdev, strerror(errno), errno);
  }

  if (hci_write_local_name(dd, deviceName, 2000) < 0)
  {
    XLOG_FATAL("Can't change local name on hci%d: %s (%d)", hdev, strerror(errno), errno);
  }

  char name[249];
  if (hci_read_local_name(dd, sizeof(name), name, 1000) < 0)
  {
    XLOG_FATAL("Can't read local name on hci%d: %s (%d)", hdev, strerror(errno), errno);
  }

  for (int i = 0; i < 248 && name[i]; i++)
  {
    if ((unsigned char) name[i] < 32 || name[i] == 127)
      name[i] = '.';
  }
  name[248] = '\0';

  XLOG_INFO("Device Name: '%s'", name);
  hci_close_dev(dd);
}

string
getDeviceNameFromFile()
{
  string str_buffer = runCommand("cat /etc/machine-info");
  if (str_buffer.find("PRETTY_HOSTNAME") != string::npos)
  {
    str_buffer = str_buffer.substr(0, str_buffer.length() - 1);
    return split(str_buffer, "=")[1];
  }
  return std::string();
}

/**
 * update device name, it need restart ble service
 * NOTE: this will run system command, and this need root permission
 * sudo sh -c "echo 'PRETTY_HOSTNAME=tc-device' > /etc/machine-info"
 * sudo service bluetooth restart
 * @param name the new device name
 */
void
updateDeviceName(string const& name)
{
  // device name is same as new name, skip it
  if (!name.compare(getDeviceNameFromFile()))
  {
    return;
  }
  char cmd_buffer[256];
  sprintf(cmd_buffer, "sh -c \"echo 'PRETTY_HOSTNAME=%s' > /etc/machine-info\"", name.c_str());
  runCommand(cmd_buffer);
  runCommand("service bluetooth restart");
  XLOG_DEBUG("device set new name = %s", getDeviceNameFromFile().c_str());
}

/**
 * send cmd to device by hci tool
 * @param dev_id  the device id
 * @param args the cmd args
 */
void
hcitoolCmd(int dev_id, vector <string> const& args)
{
  unsigned char buf[HCI_MAX_EVENT_SIZE], * ptr = buf;
  struct hci_filter flt;
  hci_event_hdr* hdr;
  int i, dd;
  int len;
  uint16_t ocf;
  uint8_t ogf;

  if (dev_id < 0)
  {
    dev_id = hci_get_route(NULL);
  }

  errno = 0;
  ogf = strtol(args[0].c_str(), NULL, 16);
  ocf = strtol(args[1].c_str(), NULL, 16);
  if (errno == ERANGE || (ogf > 0x3f) || (ocf > 0x3ff))
  {
    XLOG_FATAL("ogf must be in range (0x3f,0x3ff)");
  }

  for (i = 2, len = 0; i < (int)args.size() && len < (int) sizeof(buf); i++, len++)
    *ptr++ = (uint8_t) strtol(args[i].c_str(), NULL, 16);

  dd = hci_open_dev(dev_id);
  if (dd < 0)
  {
    XLOG_FATAL("Device open failed");
  }

  /* Setup filter */
  hci_filter_clear(&flt);
  hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
  hci_filter_all_events(&flt);
  if (setsockopt(dd, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0)
  {
    XLOG_FATAL("HCI filter setup failed");
  }

  XLOG_INFO("< HCI Command: ogf 0x%02x, ocf 0x%04x, plen %d", ogf, ocf, len);
  hex_dump(20, buf, len);
  fflush(stdout);

  if (hci_send_cmd(dd, ogf, ocf, len, buf) < 0)
  {
    XLOG_FATAL("Send failed");
  }

  len = read(dd, buf, sizeof(buf));
  if (len < 0)
  {
    XLOG_FATAL("Read failed");
  }

  hdr = static_cast<hci_event_hdr*>((void*) (buf + 1));
  ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
  len -= (1 + HCI_EVENT_HDR_SIZE);

  XLOG_INFO("> HCI Event: 0x%02x plen %d", hdr->evt, hdr->plen);
  hex_dump(20, ptr, len);
  fflush(stdout);

  hci_close_dev(dd);
}

/**
 * parse hci tool string args to vector args
 * @param str the string args
 * @return  the vector args
 */
std::vector<string>
parseArgs(string str)
{
  string delimiter = " ";

  size_t pos = 0;
  string token;
  vector <string> args;

  while ((pos = str.find(delimiter)) != string::npos)
  {
    token = str.substr(0, pos);
    args.push_back(token);
    str.erase(0, pos + delimiter.length());
  }
  args.push_back(str);
  return args;
}

/**
 * start up beacon
 * @param listenerConfig listener/beacon configuration
 */
void
startBeacon(cJSON const* listenerConfig)
{
  char const* name = JsonWrapper::getString(listenerConfig, "ble-device-name", false, "XPI-SETUP");
  int deviceId = JsonWrapper::getInt(listenerConfig, "hci-device-id", false, 0);
  int companyId = JsonWrapper::getInt(listenerConfig, "/beacon-config/company-id", false, 0xFFFF);
  int deviceInfoUUID = JsonWrapper::getInt(listenerConfig, "/beacon-config/device-info-uuid", false, 0x1000);
  int rdkDiagUUID = JsonWrapper::getInt(listenerConfig, "/beacon-config/rdk-diag-uuid", false, 0x2000);

  XLOG_INFO("starting beacon %s on hci%d", name, deviceId);
  XLOG_INFO("companyId: 0x%X, deviceInfoUUID: 0x%X, rdkDiagUUID: 0x%X", companyId, deviceInfoUUID, rdkDiagUUID);

  // updateDeviceName(s.c_str());
  struct hci_dev_info deviceInfo;
  uint8_t instanceId[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };

  int ctl = 0;
  if ((ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI)) < 0)
  {
    XLOG_FATAL("Can't open HCI socket.");
  }

  deviceInfo.dev_id = deviceId;

  if (ioctl(ctl, HCIGETDEVINFO, (void*) &deviceInfo))
  {
    XLOG_FATAL("Can't get device info");
  }

  bdaddr_t any = {0}; // BDADDR_ANY
  if (hci_test_bit(HCI_RAW, &deviceInfo.flags) && !bacmp(&deviceInfo.bdaddr, &any))
  {
    int dd = hci_open_dev(deviceInfo.dev_id);
    hci_read_bd_addr(dd, &deviceInfo.bdaddr, 1000);
    hci_close_dev(dd);
  }
  else
  {
    char addr[18];
    ba2str(&deviceInfo.bdaddr, addr);
    XLOG_INFO("bluetooh mac:%s", addr);

    for (int i = 0; i < 6; ++i)
      instanceId[i] = deviceInfo.bdaddr.b[i];
  }

  cmdDown(ctl, deviceInfo.dev_id);
  cmdUp(ctl, deviceInfo.dev_id);
  cmdNoleadv(deviceInfo.dev_id);

  // TODO: hardcoded for now to
  // [antman]: sudo hciconfig hci0 class 3a0430
  // [antman]: sudo hciconfig hci0 class
  // hci0:   Type: Primary  Bus: UART
  //    BD Address: B8:27:EB:A0:DA:2C  ACL MTU: 1021:8  SCO MTU: 64:1
  //    Class: 0x3a0430
  //    Service Classes: Networking, Capturing, Object Transfer, Audio
  //    Device Class: Audio/Video, Video Camera
  system("hciconfig hci0 class 3a0430");

  char buff[128];
#if EDDYSTONE_BEACON
  snprintf(buff, sizeof(buff), "0x08 0x0008 "
    "1f 02 01 06 03 03 aa fe 17 16 aa fe 00 e7 36 c8 80 7b f4 60 cb 41 d1 45 %02x %02x %02x %02x %02x %02x 00 00",
    instanceId[5],
    instanceId[4],
    instanceId[3],
    instanceId[2],
    instanceId[1],
    instanceId[0]);
#else // Comcast Beacon
  snprintf(buff, sizeof(buff), "0x08 0x0008 "
    "1f 02 01 06 05 03 %02x %02x %02x %02x 09 ff %02x %02x %02x %02x %02x %02x %02x %02x",
    deviceInfoUUID & 0xFF, (deviceInfoUUID >> 8) & 0xFF,
    rdkDiagUUID & 0xFF, (rdkDiagUUID >> 8) & 0xFF,
    companyId & 0xFF, (companyId >> 8) & 0xFF,
    instanceId[5],
    instanceId[4],
    instanceId[3],
    instanceId[2],
    instanceId[1],
    instanceId[0]);

#endif
 
  hcitoolCmd(deviceInfo.dev_id, parseArgs(buff));

  system("hcitool cmd 0x08 0x0006 A0 00 A0 00 00 00 00 00 00 00 00 00 00 07 00");

  #if 0
  std::string startUpCmd02(appSettings_get_ble_value("ble_init_cmd02"));
  hcitoolCmd(di.dev_id, parseArgs(startUpCmd02));
  #endif

  cmdLeadv(deviceInfo.dev_id);
  cmdName(deviceInfo.dev_id, name);
}
