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

#include <cuda_runtime.h>
#include <numeric>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include "src/extendrt/delegate/tensorrt/tensorrt_utils.h"
#include "NvInferRuntimeCommon.h"
#include "src/extendrt/delegate/tensorrt/op/spacetobatch_tensorrt.h"
#include "spacetobatch_impl.cuh"
#include "ops/space_to_batch_nd.h"

namespace mindspore::lite {
int SpaceToBatchTensorRT::IsSupport(const BaseOperatorPtr &base_operator, const std::vector<TensorInfo> &in_tensors,
                                    const std::vector<TensorInfo> &out_tensors) {
  if (in_tensors.size() != 3) {
    MS_LOG(ERROR) << "Unsupported input tensor size, size is " << in_tensors.size();
    return RET_ERROR;
  }

  if (out_tensors.size() < 1) {
    MS_LOG(ERROR) << "Unsupported output tensor size, size is " << out_tensors.size();
    return RET_ERROR;
  }
  dynamic_shape_params_.support_dynamic_ = false;
  dynamic_shape_params_.support_hw_dynamic_ = false;
  return RET_OK;
}

int SpaceToBatchTensorRT::AddInnerOp(TensorRTContext *ctx) {
  nvinfer1::ITensor *input_tensor = input(ctx, 0).trt_tensor_;
  if (input(ctx, 0).trt_tensor_->getDimensions().nbDims == DIMENSION_4D && input(ctx, 0).format_ == Format::NHWC) {
    // transpose: NHWC->NCHW
    nvinfer1::IShuffleLayer *transpose_layer_in = NHWC2NCHW(ctx, *input(ctx, 0).trt_tensor_);
    if (transpose_layer_in == nullptr) {
      MS_LOG(ERROR) << "transpose: NHWC->NCHW failed";
      return RET_ERROR;
    }
    transpose_layer_in->setName((op_name_ + "_transpose2NCHW").c_str());
    this->transpose_layer_ = transpose_layer_in;
    input_tensor = transpose_layer_in->getOutput(0);
  }
  const int *block_size_ptr = reinterpret_cast<const int *>(in_tensors_[1].Data());
  int bh = *(block_size_ptr + 0);
  int bw = *(block_size_ptr + 1);
  if (bh != bw) {
    MS_LOG(ERROR) << "block_h not equal block_w " << op_name_;
    return RET_ERROR;
  }
  const int *pad_ptr = reinterpret_cast<const int *>(in_tensors_[2].Data());
  int ph0 = *(pad_ptr + 0);
  int ph1 = *(pad_ptr + 1);
  int pw0 = *(pad_ptr + 2);
  int pw1 = *(pad_ptr + 3);
  auto in_dims = input_tensor->getDimensions();
  int in = in_dims.d[0];
  int ic = in_dims.d[1];
  int ih = in_dims.d[2];
  int iw = in_dims.d[3];

  auto plugin =
    std::make_shared<SpaceToBatchPlugin>(input_tensor->getName(), in, ic, ih, iw, bh, ph0, ph1, pw0, pw1, device_id_);
  nvinfer1::ITensor *inputTensors[] = {input_tensor};
  nvinfer1::IPluginV2Layer *space2batch_opt_layer = ctx->network()->addPluginV2(inputTensors, 1, *plugin);
  if (space2batch_opt_layer == nullptr) {
    MS_LOG(ERROR) << "add spacetobatch op failed for TensorRT.";
    return RET_ERROR;
  }
  space2batch_opt_layer->setName(op_name_.c_str());
  nvinfer1::ITensor *out_tensor = space2batch_opt_layer->getOutput(0);
  ctx->RegisterTensor(ITensorHelper{out_tensor, Format::NCHW, true}, out_tensors_[0].Name());
  this->layer_ = space2batch_opt_layer;
  return RET_OK;
}

REGISTER_TENSORRT_PLUGIN(SpaceToBatchPluginCreater);
template class TensorRTPluginCreater<SpaceToBatchPlugin>;
template <class T>
nvinfer1::PluginFieldCollection TensorRTPluginCreater<T>::field_collection_{};
template <class T>
std::vector<nvinfer1::PluginField> TensorRTPluginCreater<T>::fields_;

int SpaceToBatchPlugin::enqueue(const nvinfer1::PluginTensorDesc *inputDesc,
                                const nvinfer1::PluginTensorDesc *outputDesc, const void *const *inputs,
                                void *const *outputs, void *workspace, cudaStream_t stream) noexcept {
  return RunCudaSpaceToBatch(inputDesc, inputs, outputs, stream);
}

int SpaceToBatchPlugin::RunCudaSpaceToBatch(const nvinfer1::PluginTensorDesc *inputDesc, const void *const *inputs,
                                            void *const *outputs, cudaStream_t stream) {
  int on = in_ * bh_ * bh_;
  int oc = ic_;
  int oh = (ih_ + ph0_ + ph1_) / bh_;
  int ow = (iw_ + pw0_ + pw1_) / bh_;

  int size = in_ * ic_ * ih_ * iw_;

  CalSpaceToBatch<float>(size, static_cast<const float *>(inputs[0]), in_, ih_, iw_, ic_, on, oh, ow, oc, ph0_, ph1_,
                         pw0_, pw1_, bh_, static_cast<float *>(outputs[0]), device_id_, stream);
  return RET_OK;
}

nvinfer1::IPluginV2DynamicExt *SpaceToBatchPlugin::clone() const noexcept {
  auto *plugin = new (std::nothrow) SpaceToBatchPlugin(*this);
  if (plugin == nullptr) {
    MS_LOG(ERROR) << "new plugin failed!";
    return nullptr;
  }
  plugin->setPluginNamespace(name_space_.c_str());
  return plugin;
}

size_t SpaceToBatchPlugin::getSerializationSize() const noexcept { return sizeof(int) * 9; }

nvinfer1::DimsExprs SpaceToBatchPlugin::getOutputDimensions(int32_t index, const nvinfer1::DimsExprs *inputs,
                                                            int nbInputDims,
                                                            nvinfer1::IExprBuilder &exprBuilder) noexcept {
  nvinfer1::DimsExprs dims;
  dims.nbDims = inputs[0].nbDims;
  auto bh_mul_bh = exprBuilder.constant(bh_ * bh_);
  dims.d[0] = exprBuilder.operation(nvinfer1::DimensionOperation::kPROD, *inputs[0].d[0], *bh_mul_bh);
  dims.d[1] = inputs[0].d[1];
  auto bh = exprBuilder.constant(bh_);
  auto ph_sum = exprBuilder.constant(ph0_ + ph1_);
  auto sum0 = exprBuilder.operation(nvinfer1::DimensionOperation::kSUM, *inputs[0].d[2], *ph_sum);
  dims.d[2] = exprBuilder.operation(nvinfer1::DimensionOperation::kFLOOR_DIV, *sum0, *bh);
  auto pw_sum = exprBuilder.constant(pw0_ + pw1_);
  auto sum1 = exprBuilder.operation(nvinfer1::DimensionOperation::kSUM, *inputs[0].d[3], *pw_sum);
  dims.d[3] = exprBuilder.operation(nvinfer1::DimensionOperation::kFLOOR_DIV, *sum1, *bh);
  return dims;
}

void SpaceToBatchPlugin::serialize(void *buffer) const noexcept {
  SerializeValue(&buffer, &in_, sizeof(int));
  SerializeValue(&buffer, &ic_, sizeof(int));
  SerializeValue(&buffer, &ih_, sizeof(int));
  SerializeValue(&buffer, &iw_, sizeof(int));
  SerializeValue(&buffer, &bh_, sizeof(int));
  SerializeValue(&buffer, &ph0_, sizeof(int));
  SerializeValue(&buffer, &ph1_, sizeof(int));
  SerializeValue(&buffer, &pw0_, sizeof(int));
  SerializeValue(&buffer, &pw1_, sizeof(int));
}
REGISTER_TENSORRT_CREATOR(ops::kNameSpaceToBatchND, SpaceToBatchTensorRT)
}  // namespace mindspore::lite
