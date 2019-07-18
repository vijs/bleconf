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
#ifndef __GATT_DATA_H__
#define __GATT_DATA_H__

#include <functional>
#include <memory>

struct cJSON;

struct DeviceInfoProvider
{
  std::function< std::string () > GetSystemId;
  std::function< std::string () > GetModelNumber;
  std::function< std::string () > GetSerialNumber;
  std::function< std::string () > GetFirmwareRevision;
  std::function< std::string () > GetHardwareRevision;
  std::function< std::string () > GetSoftwareRevision;
  std::function< std::string () > GetManufacturerName;
};

struct RdkDiagProvider
{
  int rdkDiagUuid;
  std::function< std::string () > GetDeviceStatus;
  std::function< std::string () > GetFirmwareDownloadStatus;
  std::function< std::string () > GetWebPAStatus;
  std::function< std::string () > GetWiFiRadio1Status;
  std::function< std::string () > GetWiFiRadio2Status;
  std::function< std::string () > GetRFStatus;
};

struct GattServiceDataProvider
{
  struct DeviceInfoProvider deviceInfoProvider;
  struct RdkDiagProvider rdkDiagProvider;
};

class GattData
{
public:
  GattData() { }
  virtual ~GattData() { }
  virtual GattServiceDataProvider getDataProvider(cJSON const* conf) = 0;

public:
  static std::shared_ptr<GattData> create();
};

#endif
