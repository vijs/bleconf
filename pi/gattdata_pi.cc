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

#include <string.h>
#include <iostream>
#include <fstream>
#include <string>

#include "gattdata_pi.h"
#include "../defs.h"
#include "../logger.h"
#include "../util.h"
#include "../jsonwrapper.h"

std::string DIS_getSystemId();
std::string DIS_getModelNumber();
std::string DIS_getSerialNumber();
std::string DIS_getFirmwareRevision();
std::string DIS_getHardwareRevision();
std::string DIS_getSoftwareRevision();
std::string DIS_getManufacturerName();
std::string RDS_getDeviceStatus();
std::string RDS_getFirmwareDownloadStatus();
std::string RDS_getWebPAStatus();
std::string RDS_getWiFiRadio1Status();
std::string RDS_getWiFiRadio2Status();
std::string RDS_getRFStatus();

static std::string DIS_getContentFromFile(char const* fname);
static std::string DIS_getVariable(char const* fname, char const* field);
static std::vector<std::string> DIS_getDeviceInfoFromDB(std::string const& rCode);

GattDataPi* GattDataPi::instance = NULL;

GattDataPi* GattDataPi::getInstance()
{
    if (!instance)
    {
        instance = new GattDataPi();
    }

    return instance;
}

GattDataPi::GattDataPi()
{

}

GattDataPi::~GattDataPi()
{
}

GattServiceDataProvider
GattDataPi::getDataProvider(cJSON const* listenerConfig)
{
  GattServiceDataProvider provider;

  provider.deviceInfoProvider.GetSystemId = &DIS_getSystemId;
  provider.deviceInfoProvider.GetModelNumber = &DIS_getModelNumber;
  provider.deviceInfoProvider.GetSerialNumber = &DIS_getSerialNumber;
  provider.deviceInfoProvider.GetFirmwareRevision = &DIS_getFirmwareRevision;
  provider.deviceInfoProvider.GetHardwareRevision = &DIS_getHardwareRevision;
  provider.deviceInfoProvider.GetSoftwareRevision = &DIS_getSoftwareRevision;
  provider.deviceInfoProvider.GetManufacturerName = &DIS_getManufacturerName;

  provider.rdkDiagProvider.rdkDiagUuid = JsonWrapper::getInt(listenerConfig, "/beacon-config/rdk-diag-uuid", false, 0x2000);
  provider.rdkDiagProvider.GetDeviceStatus = &RDS_getDeviceStatus;
  provider.rdkDiagProvider.GetFirmwareDownloadStatus = &RDS_getFirmwareDownloadStatus;
  provider.rdkDiagProvider.GetWebPAStatus = &RDS_getWebPAStatus;
  provider.rdkDiagProvider.GetWiFiRadio1Status = &RDS_getWiFiRadio1Status;
  provider.rdkDiagProvider.GetWiFiRadio2Status = &RDS_getWiFiRadio2Status;
  provider.rdkDiagProvider.GetRFStatus = &RDS_getRFStatus;

  return provider;
}

std::string
DIS_getSystemId()
{
  return DIS_getContentFromFile("/etc/machine-id");
}

std::string
DIS_getModelNumber()
{
  return DIS_getContentFromFile("/proc/device-tree/model");
}

std::string
DIS_getSerialNumber()
{
  return DIS_getVariable("/proc/cpuinfo", "Serial");
}

std::string
DIS_getFirmwareRevision()
{
  // get version from command "uname -a"
  std::string full = runCommand("uname -a");
  size_t index = full.find(" SMP");
  if (index != std::string::npos)
  {
    return full.substr(0, index);
  }
  return full;
}

std::string
DIS_getHardwareRevision()
{
  return DIS_getVariable("/proc/cpuinfo", "Revision");
}

std::string
DIS_getSoftwareRevision()
{
  return std::string(BLECONFD_VERSION);
}

std::string
DIS_getManufacturerName()
{
  std::string rCode = DIS_getHardwareRevision();
  std::vector<std::string> deviceInfo = DIS_getDeviceInfoFromDB(rCode);

  if (deviceInfo.size())
  {
    return deviceInfo[4];
  }
  return std::string("unkown");
}

static std::string
DIS_getContentFromFile(char const* fname)
{
  std::string id;

  std::ifstream infile(fname);
  if (!std::getline(infile, id))
    id = "unknown";

  return id;
}

static std::string
DIS_getVariable(char const* file, char const* field)
{
  char buff[256];
  FILE* f = fopen(file, "r");
  while (fgets(buff, sizeof(buff) - 1, f) != nullptr)
  {
    if (strncmp(buff, field, strlen(field)) == 0)
    {
      char* p = strchr(buff, ':');
      if (p)
      {
        p++;
        while (p && isspace(*p))
          p++;
      }
      if (p)
      {
        size_t len = strlen(p);
        if (len > 0)
          p[len -1] = '\0';
        return std::string(p);
      }
    }
  }
  return std::string();
}

static std::vector<std::string>
DIS_getDeviceInfoFromDB(std::string const& rCode)
{
  std::ifstream mf("pi/devices_db");
  std::string line;

  while (std::getline(mf, line))
  {
    std::vector<std::string> t = split(line, ",");
    if (!t[0].compare(rCode))
      return t;
  }

  return std::vector<std::string>();
}

// TODO: Implement following wrappers
std::string
RDS_getDeviceStatus()
{
  return std::string("READY");
}

std::string
RDS_getFirmwareDownloadStatus()
{
  return std::string("COMPLETED");
}

std::string
RDS_getWebPAStatus()
{
  return std::string("UP");
}

std::string
RDS_getWiFiRadio1Status()
{
  return std::string("UP");
}

std::string
RDS_getWiFiRadio2Status()
{
  return std::string("UP");
}

std::string
RDS_getRFStatus()
{
  return std::string("Not Connected");
}