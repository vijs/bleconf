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
#ifndef __GATT_DATA_PI_H__
#define __GATT_DATA_PI_H__

#include "../gattdata.h"

class GattDataPi : public GattData
{
public:
  virtual ~GattDataPi();
  static GattDataPi* getInstance();
  virtual GattServiceDataProvider getDataProvider(cJSON const* conf) override;

private:
  static GattDataPi* instance;
  GattDataPi();
};

#endif
