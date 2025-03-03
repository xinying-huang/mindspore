/**
 * Copyright 2020 Huawei Technologies Co., Ltd
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
#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_STRING_PREDICT_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_STRING_PREDICT_H_

#include <vector>
#include "src/litert/lite_kernel.h"
#include "include/context.h"
#include "nnacl/predict_parameter.h"

namespace mindspore::kernel {
class PredictCPUKernel : public LiteKernel {
 public:
  PredictCPUKernel(OpParameter *parameter, const std::vector<lite::Tensor *> &inputs,
                   const std::vector<lite::Tensor *> &outputs, const lite::InnerContext *ctx)
      : LiteKernel(parameter, inputs, outputs, ctx) {}
  ~PredictCPUKernel() {}

  int Prepare() override;
  int ReSize() override;
  int Run() override;

 private:
  std::vector<LabelInfo> GetLabelInfo();
};
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_CPU_STRING_PREDICT_H_
