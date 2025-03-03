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

#include "ops/grad/multilabel_margin_loss_grad.h"
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
namespace {
abstract::ShapePtr MultilabelMarginLossGradInferShape(const PrimitivePtr &primitive,
                                                      const std::vector<AbstractBasePtr> &input_args) {
  auto op_name = primitive->name();
  auto x = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->BuildShape())[kShape];
  auto target = CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->BuildShape())[kShape];
  if ((x.size() != kDim1 && x.size() != kDim2) || (target.size() != kDim1 && target.size() != kDim2)) {
    MS_EXCEPTION(ValueError) << "For " << op_name << ", the rank of input x and target should be 1 or 2, "
                             << "while rank of x is : " << x.size() << ", rank of target is : " << target.size() << ".";
  }
  if (x != target) {
    MS_EXCEPTION(ValueError) << "For " << op_name << ", x_shape and target_shape should be the same, "
                             << "while x_shape is : " << x << ", target_shape is : " << target << ".";
  }
  int64_t reduction;
  (void)CheckAndConvertUtils::GetReductionEnumValue(primitive->GetAttr(kReduction), &reduction);
  return std::make_shared<abstract::Shape>(x);
}

TypePtr MultilabelMarginLossGradInferType(const PrimitivePtr &primitive,
                                          const std::vector<AbstractBasePtr> &input_args) {
  auto op_name = primitive->name();
  const std::set<TypePtr> valid_types1 = {kFloat16, kFloat32};
  const std::set<TypePtr> valid_types2 = {kInt32};
  std::map<std::string, TypePtr> types;
  (void)types.emplace("y_grad", input_args[kInputIndex0]->BuildType());
  (void)types.emplace("x", input_args[kInputIndex1]->BuildType());
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, valid_types1, op_name);
  auto target = input_args[kInputIndex2]->BuildType();
  (void)CheckAndConvertUtils::CheckTensorTypeValid("target", target, valid_types2, op_name);
  return input_args[kInputIndex1]->BuildType();
}
}  // namespace

MIND_API_OPERATOR_IMPL(MultilabelMarginLossGrad, BaseOperator);
AbstractBasePtr MultilabelMarginLossGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                              const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  const int64_t kInputsNum = 4;
  (void)CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, kInputsNum, primitive->name());
  auto infer_type = MultilabelMarginLossGradInferType(primitive, input_args);
  auto infer_shape = MultilabelMarginLossGradInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}
REGISTER_PRIMITIVE_EVAL_IMPL(MultilabelMarginLossGrad, prim::kPrimMultilabelMarginLossGrad,
                             MultilabelMarginLossGradInfer, nullptr, true);
}  // namespace ops
}  // namespace mindspore
