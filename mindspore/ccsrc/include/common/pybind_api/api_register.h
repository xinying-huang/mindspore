/**
 * Copyright 2019 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CCSRC_INCLUDE_COMMON_PYBIND_API_API_REGISTER_H_
#define MINDSPORE_CCSRC_INCLUDE_COMMON_PYBIND_API_API_REGISTER_H_

#include <map>
#include <string>
#include <memory>
#include <functional>
#include <vector>

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "include/common/visible.h"

namespace py = pybind11;

namespace mindspore {

using PybindDefineFunc = std::function<void(py::module *)>;

class COMMON_EXPORT PybindDefineRegister {
 public:
  static void Register(const std::string &name, const std::string &parent_name, const PybindDefineFunc &fn) {
    return GetSingleton().RegisterFn(name, parent_name, fn);
  }

  PybindDefineRegister(const PybindDefineRegister &) = delete;

  PybindDefineRegister &operator=(const PybindDefineRegister &) = delete;

  static std::map<std::string, PybindDefineFunc> &AllFuncs() { return GetSingleton().fns_; }

  static std::map<std::string, std::string> &GetInheritanceMap() { return GetSingleton().parent_name_; }

  std::map<std::string, PybindDefineFunc> fns_;

 protected:
  PybindDefineRegister() = default;

  virtual ~PybindDefineRegister() = default;

  static PybindDefineRegister &GetSingleton();

  void RegisterFn(const std::string &name, const std::string &parent_name, const PybindDefineFunc &fn) {
    parent_name_[name] = parent_name;
    fns_[name] = fn;
  }

  std::map<std::string, std::string> parent_name_;
};

class PybindDefineRegisterer {
 public:
  PybindDefineRegisterer(const std::string &name, const std::string &parent_name, const PybindDefineFunc &fn) noexcept {
    PybindDefineRegister::Register(name, parent_name, fn);
  }
  ~PybindDefineRegisterer() = default;
};

#define REGISTER_PYBIND_DEFINE(name, define) PybindDefineRegisterer g_pybind_define_f_##name(#name, "", define)

#define REGISTER_PYBIND_WITH_PARENT_NAME(name, parent_name, define) \
  PybindDefineRegisterer g_pybind_define_f_##name(#name, #parent_name, define)
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_INCLUDE_COMMON_PYBIND_API_API_REGISTER_H_
