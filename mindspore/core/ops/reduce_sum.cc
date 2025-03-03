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

#include <memory>
#include <algorithm>
#include <set>

#include "ops/reduce_sum.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
MIND_API_OPERATOR_IMPL(ReduceSum, Reduce);
AbstractBasePtr ReduceSumInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                               const std::vector<AbstractBasePtr> &input_args) {
  const int64_t input_num = 1;
  MS_EXCEPTION_IF_NULL(primitive);
  CheckAndConvertUtils::CheckInteger("input size", SizeToLong(input_args.size()), kGreaterEqual, input_num,
                                     primitive->name());
  return abstract::MakeAbstract(ReduceBaseInferShape(primitive, input_args, kNameReduceSum),
                                ReduceBaseInferType(primitive, input_args));
}

REGISTER_PRIMITIVE_C(kNameReduceSum, ReduceSum);
REGISTER_HOST_DEPENDS(kNameReduceSum, {1});
}  // namespace ops
}  // namespace mindspore
