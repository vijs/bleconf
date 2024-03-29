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
#include "gattdata.h"
#include "jsonwrapper.h"
#include "pi/gattdata_pi.h"

std::shared_ptr<GattData>
GattData::create()
{
  return std::shared_ptr<GattData>(
#ifdef PLATFORM_RASPBERRYPI
  GattDataPi::getInstance()
#endif
  );
}
