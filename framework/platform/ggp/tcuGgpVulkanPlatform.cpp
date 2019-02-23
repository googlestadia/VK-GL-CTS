/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright (c) 2014 The Android Open Source Project
 * Copyright (c) 2016 The Khronos Group Inc.
 * Copyright (c) 2016 Mun Gwan-gyeong <elongbug@gmail.com>
 * Copyright (c) 2017 Google Inc.
 * Copyright (c) 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief GGP Vulkan Platform.
 *//*--------------------------------------------------------------------*/

#include "tcuGgpVulkanPlatform.hpp"

#include "deMemory.h"
#include "deUniquePtr.hpp"
#include "tcuFunctionLibrary.hpp"
#include "tcuGgp.hpp"
#include "tcuGgpPlatform.hpp"
#include "vkWsiPlatform.hpp"

#include <ggp_c/ggp.h>
#include <sys/utsname.h>

using de::MovePtr;
using de::UniquePtr;

namespace tcu {
namespace ggp {

struct VulkanWindow : public vk::wsi::GgpWindowInterface {
  VulkanWindow()
      : vk::wsi::GgpWindowInterface(static_cast<vk::pt::GgpStreamDescriptor>(
            kGgpPrimaryStreamDescriptor)) {}
};

class VulkanDisplay : public vk::wsi::Display {
 public:
  vk::wsi::Window* createWindow(const Maybe<UVec2>& initialSize) const {
    (void)initialSize;
    return new VulkanWindow();
  }
};

class VulkanLibrary : public vk::Library {
 public:
  VulkanLibrary(void) : m_library("libvulkan.so.1"), m_driver(m_library) {}

  const vk::PlatformInterface& getPlatformInterface(void) const {
    return m_driver;
  }

  const tcu::FunctionLibrary& getFunctionLibrary(void) const {
    return m_library;
  }

 private:
  const DynamicFunctionLibrary m_library;
  const vk::PlatformDriver m_driver;
};

GgpVulkanPlatform::GgpVulkanPlatform() {}

vk::wsi::Display* GgpVulkanPlatform::createWsiDisplay(
    vk::wsi::Type wsiType) const {
  switch (wsiType) {
    case vk::wsi::TYPE_GGP:
      return new VulkanDisplay();
    default:
      TCU_THROW(NotSupportedError, "WSI type not supported");
  };
}

vk::Library* GgpVulkanPlatform::createLibrary(void) const {
  return new VulkanLibrary();
}

void GgpVulkanPlatform::describePlatform(std::ostream& dst) const {
  utsname sysInfo;
  deMemset(&sysInfo, 0, sizeof(sysInfo));

  if (uname(&sysInfo) != 0) throw std::runtime_error("uname() failed");

  dst << "OS: " << sysInfo.sysname << " " << sysInfo.release << " "
      << sysInfo.version << "\n";
  dst << "CPU: " << sysInfo.machine << "\n";
}

void GgpVulkanPlatform::getMemoryLimits(
    vk::PlatformMemoryLimits& limits) const {
  limits.totalSystemMemory = 256 * 1024 * 1024;
  limits.totalDeviceLocalMemory = 128 * 1024 * 1024;
  limits.deviceMemoryAllocationGranularity = 64 * 1024;
  limits.devicePageSize = 4096;
  limits.devicePageTableEntrySize = 8;
  limits.devicePageTableHierarchyLevels = 3;
}

}  // namespace ggp
}  // namespace tcu
