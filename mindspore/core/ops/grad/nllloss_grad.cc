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

#include "ops/grad/nllloss_grad.h"

#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
namespace {
void CheckValueIn(const std::string &arg_name, size_t value, const std::vector<size_t> &in_values,
                  const std::string &prim_name) {
  if (!std::any_of(in_values.begin(), in_values.end(), [value](size_t iv) { return iv == value; })) {
    std::ostringstream buffer;
    if (prim_name.empty()) {
      buffer << "The";
    } else {
      buffer << "For '" << prim_name << "', the";
    }

    buffer << "'" << arg_name << "'";
    buffer << " must in [";
    for (auto item : in_values) {
      buffer << item << ",";
    }
    buffer << "], but got '" << value << "' with type 'int'.";
    MS_EXCEPTION(ValueError) << buffer.str();
  }
}

void CheckNLLLossGradShapeValid(const std::string &prim_name, const ShapeVector &x_shape, const ShapeVector &t_shape,
                                const ShapeVector &w_shape) {
  if (x_shape.empty() || t_shape.empty() || w_shape.empty()) {
    return;
  }

  CheckValueIn("x rank", x_shape.size(), {1, 2}, prim_name);
  (void)CheckAndConvertUtils::CheckInteger("target rank", SizeToLong(t_shape.size()), kEqual, 1, prim_name);
  (void)CheckAndConvertUtils::CheckInteger("weight rank", SizeToLong(w_shape.size()), kEqual, 1, prim_name);
  if (x_shape.size() == 1) {
    CheckAndConvertUtils::Check("target_shape", 1, kEqual, t_shape[0], prim_name);
    CheckAndConvertUtils::Check("weight_shape", x_shape[0], kEqual, w_shape[0], prim_name);
  } else {
    CheckAndConvertUtils::Check("target_shape", x_shape[0], kEqual, t_shape[0], prim_name);
    CheckAndConvertUtils::Check("weight_shape", x_shape[1], kEqual, w_shape[0], prim_name);
  }
}
}  // namespace

void NLLLossGrad::Init(const Reduction &reduction) { set_reduction(reduction); }

void NLLLossGrad::set_reduction(const Reduction &reduction) {
  int64_t reduce = reduction;
  (void)AddAttr(kReduction, api::MakeValue(reduce));
}

Reduction NLLLossGrad::get_reduction() const {
  auto value_ptr = GetAttr(kReduction);
  return Reduction(GetValue<int64_t>(value_ptr));
}

abstract::ShapePtr NLLLossGradInferShape(const PrimitivePtr &primitive,
                                         const std::vector<AbstractBasePtr> &input_args) {
  auto prim_name = primitive->name();

  // Check valid.
  const size_t x_idx = 0;
  const size_t t_idx = 2;
  const size_t w_idx = 3;
  (void)CheckAndConvertUtils::CheckArgs<abstract::AbstractTensor>(prim_name, input_args, x_idx);
  auto x = input_args[x_idx]->BuildShape();
  MS_EXCEPTION_IF_NULL(x);
  (void)CheckAndConvertUtils::CheckArgs<abstract::AbstractTensor>(prim_name, input_args, t_idx);
  auto t = input_args[t_idx]->BuildShape();
  MS_EXCEPTION_IF_NULL(t);
  (void)CheckAndConvertUtils::CheckArgs<abstract::AbstractTensor>(prim_name, input_args, w_idx);
  auto w = input_args[w_idx]->BuildShape();
  MS_EXCEPTION_IF_NULL(w);

  auto x_shape = x->cast<abstract::ShapePtr>();
  MS_EXCEPTION_IF_NULL(x_shape);

  if (x->IsDynamic() || t->IsDynamic() || w->IsDynamic()) {
    return x_shape;
  }

  auto t_shape = t->cast<abstract::ShapePtr>();
  MS_EXCEPTION_IF_NULL(t_shape);
  auto w_shape = w->cast<abstract::ShapePtr>();
  MS_EXCEPTION_IF_NULL(w_shape);

  CheckNLLLossGradShapeValid(prim_name, x_shape->shape(), t_shape->shape(), w_shape->shape());

  return x_shape;
}

TypePtr NLLLossGradInferType(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  // check
  std::set<TypePtr> valid_types = {kFloat16, kFloat32};
  auto x_dtype = input_args[kInputIndex0]->BuildType();
  auto y_grad_dtype = input_args[kInputIndex1]->BuildType();
  auto t_dtype = input_args[kInputIndex2]->BuildType();
  auto w_dtype = input_args[kInputIndex3]->BuildType();
  auto tw_dtype = input_args[kInputIndex4]->BuildType();
  (void)CheckAndConvertUtils::CheckTensorTypeValid("x_dtype", x_dtype, valid_types, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("y_grad_dtype", y_grad_dtype, valid_types, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("t_dtype", t_dtype, {kInt32}, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("w_dtype", w_dtype, valid_types, prim_name);
  (void)CheckAndConvertUtils::CheckTensorTypeValid("tw_dtype", tw_dtype, valid_types, prim_name);
  CheckAndConvertUtils::Check("w_dtype", std::vector<TypeId>{tw_dtype->type_id()}, kEqual,
                              std::vector<TypeId>{w_dtype->type_id()}, prim_name);
  return x_dtype;
}

AbstractBasePtr NLLLossGradInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                 const std::vector<AbstractBasePtr> &input_args) {
  MS_EXCEPTION_IF_NULL(primitive);
  auto prim_name = primitive->name();
  const int64_t input_num = 5;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, prim_name);
  auto types = NLLLossGradInferType(primitive, input_args);
  auto shapes = NLLLossGradInferShape(primitive, input_args);
  return abstract::MakeAbstract(shapes, types);
}

MIND_API_OPERATOR_IMPL(NLLLossGrad, BaseOperator);
REGISTER_PRIMITIVE_EVAL_IMPL(NLLLossGrad, std::make_shared<Primitive>("NLLLossGrad"), NLLLossGradInfer, nullptr, true);
}  // namespace ops
}  // namespace mindspore
