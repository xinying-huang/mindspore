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

#include "ops/sparse_matrix_mat_mul.h"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "ops/op_utils.h"
#include "utils/check_convert_utils.h"
#include "abstract/ops/primitive_infer_map.h"
#include "mindapi/src/helper.h"

namespace mindspore {
namespace ops {
namespace {
void SparseMatrixMatMulCheckShape(const PrimitivePtr &primitive, const std::vector<AbstractBasePtr> &input_args) {
  std::vector<int64_t> x1_dense_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->BuildShape())[kShape];
  const int64_t rank_x1 = x1_dense_shape[0];
  std::vector<int64_t> x1_batch_pointer =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->BuildShape())[kShape];
  std::vector<int64_t> x1_row_pointer =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex2]->BuildShape())[kShape];
  std::vector<int64_t> x1_col_indices =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex3]->BuildShape())[kShape];
  std::vector<int64_t> x1_values =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex4]->BuildShape())[kShape];
  std::vector<int64_t> x2_dense_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex5]->BuildShape())[kShape];
  const int64_t rank_x2 = x2_dense_shape.size();
  if (rank_x1 != rank_x2) {
    MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, x1_dense_shape.shape[0] and rank of x2_dense must be the "
                                "same, but got x1_dense_shape.shape[0] = "
                             << rank_x1 << ", and rank of x2_dense = " << rank_x2 << ".";
  }  // check rank

  const int kInputNoBatch = 2;
  const int kInputWithBatch = 3;
  const int kOne = 1;
  if (x1_dense_shape.size() != kOne) {
    MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, x1_dense_shape should be 1-D, bug got "
                             << x1_dense_shape.size() << "-D.";
  }
  if (x1_batch_pointer.size() != kOne) {
    MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, x1_batch_pointers should be 1-D, bug got "
                             << x1_batch_pointer.size() << "-D.";
  }
  if (x1_row_pointer.size() != kOne) {
    MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, x1_row_pointers should be 1-D, bug got "
                             << x1_row_pointer.size() << "-D.";
  }
  if (x1_col_indices.size() != kOne) {
    MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, x1_col_indices should be 1-D, bug got "
                             << x1_col_indices.size() << "-D.";
  }
  if (x1_values.size() != kOne) {
    MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, x1_values should be 1-D, bug got " << x1_values.size()
                             << "-D.";
  }
  if (rank_x1 != kInputNoBatch && rank_x1 != kInputWithBatch) {
    MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, rank of x1_dense_shape must be (2,) or (3,), but got "
                             << rank_x1 << ".";
  }
  if (rank_x2 == kInputWithBatch) {
    size_t x1_batch_num =
      CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex1]->BuildShape())[kShape][0] - 1;
    size_t x2_batch_num =
      CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex5]->BuildShape())[kShape][0];
    if (x1_batch_num != x2_batch_num) {
      MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, x1_dense_shape[0] and x2_dense.shape[0] must be the "
                                  "same, but got x1_dense_shape[0] = "
                               << x1_batch_num << ", and x2_dense.shape[0] = " << x2_batch_num << ".";
    }
  }
}

abstract::ShapePtr SparseMatrixMatMulInferShape(const PrimitivePtr &primitive,
                                                const std::vector<AbstractBasePtr> &input_args) {
  SparseMatrixMatMulCheckShape(primitive, input_args);
  std::vector<int64_t> x1_dense_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex0]->BuildShape())[kShape];
  const int64_t rank_x1 = x1_dense_shape[0];
  std::vector<int64_t> x2_dense_shape =
    CheckAndConvertUtils::ConvertShapePtrToShapeMap(input_args[kInputIndex5]->BuildShape())[kShape];
  const int64_t rank_x2 = x2_dense_shape.size();
  ShapeVector y_dense_shape;
  auto transpose_x1 = GetValue<bool>(primitive->GetAttr("transpose_x1"));
  auto transpose_x2 = GetValue<bool>(primitive->GetAttr("transpose_x2"));
  auto adjoint_x1 = GetValue<bool>(primitive->GetAttr("adjoint_x1"));
  auto adjoint_x2 = GetValue<bool>(primitive->GetAttr("adjoint_x2"));
  auto transpose_output = GetValue<bool>(primitive->GetAttr("transpose_output"));
  auto conjugate_output = GetValue<bool>(primitive->GetAttr("conjugate_output"));
  if (transpose_output && conjugate_output) {
    MS_EXCEPTION(ValueError) << "For SparseMatrixMatMul, only one of transpose_output and conjugate_output may be "
                                "true, but got transpose_output = "
                             << transpose_output << ", and conjugate_output = " << conjugate_output << ".";
  }

  // row and col of B
  int64_t row_x2 = rank_x2 == 2 ? x2_dense_shape[0] : x2_dense_shape[1];
  int64_t col_x2 = rank_x2 == 2 ? x2_dense_shape[1] : x2_dense_shape[2];
  if (adjoint_x2 && transpose_x2) {
    MS_EXCEPTION(ValueError)
      << "For SparseMatrixMatMul, only one of adjoint_x2 and transpose_x2 may be true, but got adjoint_x2 = "
      << adjoint_x2 << ", and transpose_x2 = " << transpose_x2 << ".";
  }
  col_x2 = (adjoint_x2 || transpose_x2) ? row_x2 : col_x2;

  // row and col of A
  const int kInputWithBatch = 3;
  if (input_args[0]->isa<abstract::AbstractTensor>() && !input_args[0]->BuildValue()->isa<AnyValue>() &&
      !input_args[0]->BuildValue()->isa<None>()) {
    auto dense_shape_value = input_args[0]->cast<abstract::AbstractTensorPtr>();
    MS_EXCEPTION_IF_NULL(dense_shape_value);
    auto dense_shape_value_ptr = dense_shape_value->BuildValue();
    MS_EXCEPTION_IF_NULL(dense_shape_value_ptr);
    auto dense_shape_value_ptr_tensor =
      CheckAndConvertUtils::CheckTensorIntValue("x1_dense_shape", dense_shape_value_ptr, primitive->name());
    auto row_x1 = static_cast<int64_t>(*(dense_shape_value_ptr_tensor.end() - 2));
    auto col_x1 = static_cast<int64_t>(*(dense_shape_value_ptr_tensor.end() - 1));

    if (adjoint_x1 && transpose_x1) {
      MS_EXCEPTION(ValueError)
        << "For SparseMatrixMatMul, only one of adjoint_x1 and transpose_x1 may be true, but got adjoint_x2 = "
        << adjoint_x1 << ", and transpose_x1 = " << transpose_x1 << ".";
    }
    row_x1 = (adjoint_x1 || transpose_x1) ? col_x1 : row_x1;

    size_t row_y = row_x1;
    size_t col_y = col_x2;
    if (transpose_output) {
      int temp = col_y;
      col_y = row_y;
      row_y = temp;
    }

    if (rank_x1 == kInputWithBatch) {
      y_dense_shape.push_back(x2_dense_shape[0]);
    }
    y_dense_shape.push_back(row_y);
    y_dense_shape.push_back(col_y);
    return std::make_shared<abstract::Shape>(y_dense_shape);
  } else {
    ShapeVector dense_shape = {-2};
    ShapeVector infer_shape_min;
    ShapeVector infer_shape_max;
    infer_shape_min = infer_shape_max = {1};
    return std::make_shared<abstract::Shape>(dense_shape, infer_shape_min, infer_shape_max);
  }
}

TypePtr SparseMatrixMatMulInferType(const PrimitivePtr &prim, const std::vector<AbstractBasePtr> &input_args) {
  const std::set<TypePtr> index_valid_types = {kInt32, kInt64};
  const std::set<TypePtr> values_valid_types = {kFloat32, kFloat64, kComplex64, kComplex128};
  auto x1_dense_type = input_args[kInputIndex0]->BuildType();
  auto x1_batch_type = input_args[kInputIndex1]->BuildType();
  auto x1_row_type = input_args[kInputIndex2]->BuildType();
  auto x1_col_type = input_args[kInputIndex3]->BuildType();
  auto x1_values_type = input_args[kInputIndex4]->BuildType();
  auto x2_values_type = input_args[kInputIndex5]->BuildType();
  std::map<std::string, TypePtr> types_values;
  (void)types_values.emplace("x1_values", x1_values_type);
  (void)types_values.emplace("x2_dense", x2_values_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types_values, values_valid_types, prim->name());
  std::map<std::string, TypePtr> types;
  (void)types.emplace("x1_dense_shape", x1_dense_type);
  (void)types.emplace("x1_batch_pointers", x1_batch_type);
  (void)types.emplace("x1_row_pointers", x1_row_type);
  (void)types.emplace("x1_col_indices", x1_col_type);
  (void)CheckAndConvertUtils::CheckTensorTypeSame(types, index_valid_types, prim->name());
  return x1_values_type;
}
}  // namespace

MIND_API_BASE_IMPL(SparseMatrixMatMul, PrimitiveC, BaseOperator);
AbstractBasePtr SparseMatrixMatMulInfer(const abstract::AnalysisEnginePtr &, const PrimitivePtr &primitive,
                                        const std::vector<AbstractBasePtr> &input_args) {
  const int64_t input_num = 6;
  CheckAndConvertUtils::CheckInputArgs(input_args, kEqual, input_num, primitive->name());
  auto infer_type = SparseMatrixMatMulInferType(primitive, input_args);
  auto infer_shape = SparseMatrixMatMulInferShape(primitive, input_args);
  return abstract::MakeAbstract(infer_shape, infer_type);
}
REGISTER_PRIMITIVE_EVAL_IMPL(SparseMatrixMatMul, prim::kPrimSparseMatrixMatMul, SparseMatrixMatMulInfer, nullptr, true);
REGISTER_HOST_DEPENDS(kNameSparseMatrixMatMul, {0});
}  // namespace ops
}  // namespace mindspore
