/**
 * Copyright 2021-2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SCATTER_ARITHMETIC_CPU_KERNEL_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SCATTER_ARITHMETIC_CPU_KERNEL_H_

#include <vector>
#include <string>
#include <memory>
#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"

namespace mindspore {
namespace kernel {
constexpr auto kScatterAdd = "ScatterAdd";
constexpr auto kScatterSub = "ScatterSub";
constexpr auto kScatterMul = "ScatterMul";
constexpr auto kScatterDiv = "ScatterDiv";
constexpr auto kScatterMax = "ScatterMax";
constexpr auto kScatterMin = "ScatterMin";
constexpr auto kScatterUpdate = "ScatterUpdate";
constexpr auto kUnKnown = "UnKnown";
class ScatterArithmeticCpuKernelMod : public DeprecatedNativeCpuKernelMod {
 public:
  ScatterArithmeticCpuKernelMod() = default;

  explicit ScatterArithmeticCpuKernelMod(const std::string &kernel_type) : kernel_type_(kernel_type) {}
  ~ScatterArithmeticCpuKernelMod() override = default;

  void InitKernel(const CNodePtr &kernel_node) override;

  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs) override {
    return func_obj_->RunFunc(inputs, workspace, outputs);
  }

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  std::shared_ptr<DeprecatedCpuKernelFunc> func_obj_;
  std::string kernel_type_{kUnKnown};
};
}  // namespace kernel
}  // namespace mindspore

#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_SCATTER_ARITHMETIC_CPU_KERNEL_H_
