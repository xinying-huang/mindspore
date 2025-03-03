/**
 * Copyright 2022 Huawei Technologies Co., Ltd
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

#ifndef MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DENSETODENSE_SET_OPERATION_H_
#define MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DENSETODENSE_SET_OPERATION_H_

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "plugin/device/cpu/kernel/cpu_kernel.h"
#include "plugin/factory/ms_factory.h"
#include "utils/ms_utils.h"

namespace mindspore {
namespace kernel {
enum SetOperation { A_MINUS_B = 0, B_MINUS_A = 1, INTERSECTION = 2, UNION = 3 };
class DenseToDenseSetOperationCpuKernelMod : public DeprecatedNativeCpuKernelMod {
 public:
  DenseToDenseSetOperationCpuKernelMod() = default;
  ~DenseToDenseSetOperationCpuKernelMod() = default;
  void InitKernel(const CNodePtr &kernel_node) override;
  bool Launch(const std::vector<AddressPtr> &inputs, const std::vector<AddressPtr> &workspace,
              const std::vector<AddressPtr> &outputs) override;

  std::vector<KernelAttr> GetOpSupport() override;

 private:
  template <typename T>
  bool LaunchKernel(const std::vector<kernel::AddressPtr> &inputs, const std::vector<kernel::AddressPtr> &outputs);
  template <typename T>
  bool PopulateOutput(const std::vector<kernel::AddressPtr> &inputs, const std::vector<kernel::AddressPtr> &outputs,
                      const ShapeVector &output_shape, const size_t num_values,
                      const std::map<std::vector<size_t>, std::set<T>> *sets);
  template <typename T>
  void SetCompute(const std::set<T> &set1, const std::set<T> &set2, std::set<T> *result);
  SetOperation set_operation_ = A_MINUS_B;
  bool validate_indices_ = true;
  CNodePtr node_ptr_;
};
}  // namespace kernel
}  // namespace mindspore
#endif  // MINDSPORE_CCSRC_BACKEND_KERNEL_COMPILER_CPU_DENSETODENSE_SET_OPERATION_H_
