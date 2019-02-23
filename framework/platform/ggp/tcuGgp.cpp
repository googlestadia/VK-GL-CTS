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
 * \brief GGP utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuGgp.hpp"

namespace tcu {
namespace ggp {

EventState::EventState(void) : m_quit(false) {}

EventState::~EventState(void) {}

void EventState::setQuitFlag(bool quit) {
  de::ScopedLock lock(m_mutex);
  m_quit = quit;
}

bool EventState::getQuitFlag(void) {
  de::ScopedLock lock(m_mutex);
  return m_quit;
}

}  // ggp
}  // tcu
