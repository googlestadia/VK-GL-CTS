#ifndef _TCUGGPVULKANPLATFORM_HPP
#define _TCUGGPVULKANPLATFORM_HPP
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

#include "vkWsiPlatform.hpp"

#include "tcuGgp.hpp"
#include "vkPlatform.hpp"

namespace tcu {
namespace ggp {

class GgpVulkanPlatform : public vk::Platform {
 public:
  GgpVulkanPlatform();
  vk::wsi::Display* createWsiDisplay(vk::wsi::Type wsiType) const;
  vk::Library* createLibrary(void) const;
  void describePlatform(std::ostream& dst) const;
  void getMemoryLimits(vk::PlatformMemoryLimits& limits) const;
};

}  // ggp
}  // tcu

#endif  // _TCUGGPVULKANPLATFORM_HPP
