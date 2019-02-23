#ifndef _TCUGGP_HPP
#define _TCUGGP_HPP
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

#include "tcuDefs.hpp"

#include "deMutex.hpp"

namespace tcu {
namespace ggp {

enum { DEFAULT_WINDOW_WIDTH = 1920, DEFAULT_WINDOW_HEIGHT = 1080 };

class EventState {
 public:
  EventState(void);
  virtual ~EventState(void);

  void setQuitFlag(bool quit);
  bool getQuitFlag(void);

 protected:
  de::Mutex m_mutex;
  bool m_quit;

 private:
  EventState(const EventState&);
  EventState& operator=(const EventState&);
};

}  // ggp
}  // tcu

#endif  // _TCUGGP_HPP
