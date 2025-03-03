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

#include <algorithm>
#include <utility>
#include "plugin/device/ascend/kernel/tbe/tiling/op_tiling_adapter.h"
#include "plugin/device/ascend/kernel/tbe/tbe_kernel_build.h"
#include "plugin/device/ascend/kernel/tbe/tbe_dynamic_shape_util.h"
#include "backend/common/session/anf_runtime_algorithm.h"
#include "include/common/utils/anfalgo.h"
#include "plugin/device/ascend/hal/device/ge_types_convert.h"
#include "include/common/utils/utils.h"
#include "external/graph/tensor.h"
#include "external/register/op_tiling_registry.h"
#include "graph/utils/graph_utils.h"
#include "common/ge_inner_error_codes.h"
#include "graph/utils/op_desc_utils.h"
#include "plugin/device/ascend/kernel/ascend_kernel_mod.h"
#include "graph/utils/tensor_utils.h"

namespace mindspore {
namespace device {
namespace tiling {
constexpr auto COMPILE_INFO_KEY = "compile_info_key";
constexpr auto COMPILE_INFO_JSON = "compile_info_json";
constexpr auto ATOMIC_COMPILE_INFO_KEY = "_atomic_compile_info_key";
constexpr auto ATOMIC_COMPILE_INFO_JSON = "_atomic_compile_info_json";
constexpr auto ATTR_NAME_OP_INFER_DEPENDS = "_op_infer_depends";
constexpr auto CONSTANTOP = "Constant";
constexpr auto ATTR_NAME_WEIGHTS = "value";
constexpr auto PARAM_DYNAMIC = "dynamic";
constexpr auto EXT_ATTR_ATOMIC_WORKSPACE_INFO = "sub_node_workspace_info";

std::string OpTilingCalculateAdapter::GetRealOpType(const std::string &op_type) const {
  static const std::map<std::string, std::string> kOpTypeMap = {
    {"SparseApplyFtrl", "SparseApplyFtrlD"},
    {"SparseApplyProximalAdagrad", "SparseApplyProximalAdagradD"},
    {"SparseGatherV2", "Gather"},
    {"Pad", "PadD"},
    {"Split", "SplitD"},
    {"Concat", "ConcatD"},
    {"Softmax", "SoftmaxV2"},
    {"DropoutDoMask", "DropOutDoMask"},
    {"IOU", "Iou"},
    {"DynamicBroadcastTo", "BroadcastTo"},
    {"DynamicResizeNearestNeighbor", "ResizeNearestNeighborV2"},
    {"ParallelResizeBilinear", "SyncResizeBilinearV2"},
    {"ParallelResizeBilinearGrad", "SyncResizeBilinearV2Grad"},
    {"ResizeBilinearGrad", "ResizeBilinearV2Grad"},
    {"HSwish", "HardSwish"},
    {"HSwishGrad", "HardSwishGrad"},
    {"CeLU", "CeluV2"},
    {"TransposeNOD", "Transpose"},
    {"IndexAdd", "InplaceIndexAdd"},
    {"KLDivLoss", "KLDiv"},
    {"Unstack", "Unpack"},
    {"ArgminV2", "ArgMin"},
    {"InplaceUpdateV2", "InplaceUpdate"},
  };
  auto iter = kOpTypeMap.find(op_type);
  if (iter == kOpTypeMap.end()) {
    return op_type;
  }
  return iter->second;
}

std::map<std::string, std::string> OpTilingCalculateAdapter::GetConvertAttr(const std::string &op_type) const {
  std::map<std::string, std::string> attrs;
  static const std::map<std::string, std::map<std::string, std::string>> op_type_map = {
    {"ArgMaxWithValue", {{"axis", "dimension"}}},
    {"ArgMinWithValue", {{"axis", "dimension"}}},
    {"DepthToSpace", {{"block_size", "block_size"}}}};
  auto iter = op_type_map.find(op_type);
  return iter == op_type_map.end() ? attrs : iter->second;
}

std::string OpTilingCalculateAdapter::GetOutputName(const CNodePtr &node, size_t index) {
  MS_EXCEPTION_IF_NULL(node);
  if (output_names_.size() <= index) {
    return "unknown_name";
  }
  return output_names_[index];
}

std::string OpTilingCalculateAdapter::GetInputName(const CNodePtr &node, size_t index) {
  MS_EXCEPTION_IF_NULL(node);
  if (input_names_.size() <= index) {
    return "unknown_name";
  }
  return input_names_[index];
}

void OpTilingCalculateAdapter::ConvertInputShapeAndType(const CNodePtr &node, ::ge::OpDescPtr *op_desc) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*op_desc);
  auto input_size = common::AnfAlgo::GetInputTensorNum(node);
  for (size_t i = 0; i < input_size; i++) {
    // ms info
    auto real_index = AnfAlgo::GetInputIndexInGraph(node, i);
    auto input_node_with_idx = common::AnfAlgo::GetPrevNodeOutput(node, real_index);
    auto input_node = input_node_with_idx.first;
    auto input_index = input_node_with_idx.second;
    auto ms_ori_shape = common::AnfAlgo::GetOutputInferShape(input_node, input_index);
    auto ms_shape = AnfAlgo::GetOutputDeviceShape(input_node, input_index);
    auto ms_format = AnfAlgo::GetOutputFormat(input_node, input_index);
    auto ms_dtype = AnfAlgo::GetOutputDeviceDataType(input_node, input_index);

    // ge info
    ::ge::DataType ge_dtype = ascend::GeTypesConvert::TransTypeIdToGeDataType(ms_dtype);
    ::ge::Format ge_format = ascend::GeTypesConvert::GetGeFormat(ms_format, ms_shape.size());

    auto input_name = GetInputName(node, real_index);
    ::ge::GeTensorDesc ge_tensor_desc;
    ge_tensor_desc.SetFormat(ge_format);
    ge_tensor_desc.SetDataType(ge_dtype);
    ge_tensor_desc.SetShape(::ge::GeShape(ms_shape));
    ge_tensor_desc.SetOriginShape(::ge::GeShape(ms_ori_shape));
    ge_tensor_desc.SetName(input_name);
    (void)(*op_desc)->AddInputDesc(input_name, ge_tensor_desc);
  }
}

void OpTilingCalculateAdapter::ConvertOutputShapeAndType(const CNodePtr &node, ::ge::OpDescPtr *op_desc) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*op_desc);
  auto output_size = common::AnfAlgo::GetOutputTensorNum(node);
  for (size_t i = 0; i < output_size; i++) {
    auto ms_shape = AnfAlgo::GetOutputDeviceShape(node, i);
    auto ms_ori_shape = common::AnfAlgo::GetOutputInferShape(node, i);
    auto ms_format = AnfAlgo::GetOutputFormat(node, i);
    auto ms_dtype = AnfAlgo::GetOutputDeviceDataType(node, i);

    ::ge::DataType ge_dtype = ascend::GeTypesConvert::TransTypeIdToGeDataType(ms_dtype);
    ::ge::Format ge_format = ascend::GeTypesConvert::GetGeFormat(ms_format, ms_shape.size());

    auto output_name = GetOutputName(node, i);
    ::ge::GeTensorDesc ge_tensor_desc;
    ge_tensor_desc.SetFormat(ge_format);
    ge_tensor_desc.SetDataType(ge_dtype);
    ge_tensor_desc.SetShape(::ge::GeShape(ms_shape));
    ge_tensor_desc.SetOriginShape(::ge::GeShape(ms_ori_shape));
    ge_tensor_desc.SetName(output_name);
    (void)(*op_desc)->AddOutputDesc(output_name, ge_tensor_desc);
  }
}

void OpTilingCalculateAdapter::ConvertAttrs(const CNodePtr &node, ::ge::OpDescPtr *op_desc) const {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*op_desc);
  auto primitive = GetCNodePrimitive(node);
  if (primitive == nullptr) {
    return;
  }
  auto to_convert_attr = GetConvertAttr(primitive->name());
  if (to_convert_attr.empty()) {
    MS_LOG(DEBUG) << "The attrs of node " << primitive->name() << " does not need to be converted.";
    return;
  }
  for (const auto &attr : primitive->attrs()) {
    auto &key = attr.first;
    auto iter = to_convert_attr.find(key);
    if (iter == to_convert_attr.end()) {
      continue;
    }
    auto &new_key = iter->second;
    auto &value = attr.second;
    MS_EXCEPTION_IF_NULL(value);
    // Should add more types.
    if (value->isa<Int64Imm>()) {
      (void)::ge::AttrUtils::SetInt(*(*op_desc), new_key, GetValue<int64_t>(value));
    } else if (value->isa<StringImm>()) {
      (void)::ge::AttrUtils::SetStr(*(*op_desc), new_key, GetValue<string>(value));
    } else if (value->isa<FP32Imm>()) {
      (void)::ge::AttrUtils::SetFloat(*(*op_desc), new_key, GetValue<float>(value));
    } else if (value->isa<BoolImm>()) {
      (void)::ge::AttrUtils::SetBool(*(*op_desc), new_key, GetValue<bool>(value));
    } else {
      MS_LOG(EXCEPTION) << "Currently not support to convert the attr '" << key << "' with value: " << value->ToString()
                        << ", perhaps you should add more supported type.";
    }
  }
}

void OpTilingCalculateAdapter::ConvertCompileInfo(const CNodePtr &node, ::ge::OpDescPtr *op_desc) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*op_desc);
  MS_LOG(INFO) << "For op " << op_name_ << ", get compile_info: " << op_compile_info_;
  std::string compile_info_key = std::to_string(std::hash<std::string>()(op_compile_info_));
  (void)::ge::AttrUtils::SetStr(*(*op_desc), COMPILE_INFO_KEY, compile_info_key);
  (void)::ge::AttrUtils::SetStr(*(*op_desc), COMPILE_INFO_JSON, op_compile_info_);
}

void OpTilingCalculateAdapter::ConvertAtomicCompileInfo(const CNodePtr &node, ::ge::OpDescPtr *op_desc) const {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*op_desc);
  auto kernel_mod = dynamic_cast<kernel::AscendKernelMod *>(AnfAlgo::GetKernelMod(node));
  MS_EXCEPTION_IF_NULL(kernel_mod);
  bool has_output = common::AnfAlgo::HasNodeAttr(kAttrAtomicOutputIndexs, node);
  bool has_workspace = common::AnfAlgo::HasNodeAttr(kAttrAtomicWorkspaceIndexs, node);
  constexpr size_t kAlignBtyes = 32;
  // set atomic compile info
  if (has_output || has_workspace) {
    std::string atomic_compile_info = kernel_mod->GetAtomicCompileInfo();
    std::string atomic_info_key = std::to_string(std::hash<std::string>()(atomic_compile_info));
    (void)::ge::AttrUtils::SetStr(*(*op_desc), ATOMIC_COMPILE_INFO_KEY, atomic_info_key);
    (void)::ge::AttrUtils::SetStr(*(*op_desc), ATOMIC_COMPILE_INFO_JSON, atomic_compile_info);
  }
  // clean output
  if (has_output) {
    vector<int64_t> output_indexs;
    auto help = common::AnfAlgo::GetNodeAttr<std::vector<size_t>>(node, kAttrAtomicOutputIndexs);
    std::transform(help.begin(), help.end(), std::back_inserter(output_indexs), SizeToLong);
    ::ge::AttrUtils::SetListInt(*(*op_desc), ::ge::ATOMIC_ATTR_OUTPUT_INDEX, output_indexs);
    auto output_mem_size = kernel_mod->GetOutputSizeList();
    for (auto index : output_indexs) {
      auto output_size = static_cast<int64_t>((output_mem_size.at(index) + kMemAlignSize + kAlignBtyes - 1) /
                                              kMemAlignSize * kMemAlignSize);
      auto output = (*op_desc)->MutableOutputDesc(index);
      MS_EXCEPTION_IF_NULL(output);
      ::ge::TensorUtils::SetSize(*output, output_size);
    }
  }

  // clean workspace
  if (has_workspace) {
    auto workspace_men_sizes = kernel_mod->GetWorkspaceSizeList();
    std::vector<int64_t> workspace_list;
    std::transform(workspace_men_sizes.begin(), workspace_men_sizes.end(), std::back_inserter(workspace_list),
                   SizeToLong);
    (void)(*op_desc)->SetWorkspaceBytes(workspace_list);
    std::map<std::string, std::map<int64_t, int64_t>> workspace_info;
    std::map<int64_t, int64_t> clean_size_list;
    auto workspace_indexes = common::AnfAlgo::GetNodeAttr<std::vector<size_t>>(node, kAttrAtomicWorkspaceIndexs);
    for (const auto &index : workspace_indexes) {
      auto clean_item = static_cast<int64_t>((workspace_list.at(index) + kMemAlignSize + kAlignBtyes - 1) /
                                             kMemAlignSize * kMemAlignSize);
      clean_size_list.insert(std::make_pair(static_cast<int64_t>(index), clean_item));
    }
    workspace_info.insert(std::make_pair((*op_desc)->GetName(), clean_size_list));
    (void)(*op_desc)->SetExtAttr(EXT_ATTR_ATOMIC_WORKSPACE_INFO, workspace_info);
  }
}

::ge::NodePtr OpTilingCalculateAdapter::NewConstantOp(const CNodePtr &node, const std::string &name,
                                                      const tensor::TensorPtr &tensor_data,
                                                      ::ge::ComputeGraphPtr *ge_graph, size_t index) const {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*ge_graph);
  MS_EXCEPTION_IF_NULL(tensor_data);
  ::ge::OpDescPtr op_desc = std::make_shared<::ge::OpDesc>(name, CONSTANTOP);
  auto ms_format = AnfAlgo::GetPrevNodeOutputFormat(node, index);
  auto ge_ori_shape = common::AnfAlgo::GetPrevNodeOutputInferShape(node, index);
  auto ms_dtype = AnfAlgo::GetPrevNodeOutputDeviceDataType(node, index);
  auto ge_shape = AnfAlgo::GetInputDeviceShape(node, index);

  ::ge::DataType ge_dtype = ascend::GeTypesConvert::TransTypeIdToGeDataType(ms_dtype);
  ::ge::Format ge_format = ascend::GeTypesConvert::GetGeFormat(ms_format, ge_shape.size());
  ::ge::GeTensorDesc ge_tensor_desc;
  ge_tensor_desc.SetFormat(ge_format);
  ge_tensor_desc.SetDataType(ge_dtype);
  ge_tensor_desc.SetShape(::ge::GeShape(ge_shape));
  ge_tensor_desc.SetOriginShape(::ge::GeShape(ge_ori_shape));
  ge_tensor_desc.SetName(name);
  ::ge::GeTensorPtr ge_tensor = std::make_shared<::ge::GeTensor>(
    ge_tensor_desc, static_cast<uint8_t *>(tensor_data->data_c()), IntToSize(tensor_data->Size()));
  (void)op_desc->AddOutputDesc(name, ge_tensor_desc);
  ::ge::NodePtr constant_op = (*ge_graph)->AddNode(op_desc);
  ::ge::OpDescUtils::SetWeights(constant_op, {ge_tensor});
  (void)::ge::AttrUtils::SetTensor(op_desc, ATTR_NAME_WEIGHTS, ge_tensor);
  return constant_op;
}

std::vector<std::tuple<std::size_t, ::ge::NodePtr>> OpTilingCalculateAdapter::ConvertDepends(
  const CNodePtr &node, const std::map<uint32_t, tensor::TensorPtr> &depend_tensor_map, ::ge::OpDescPtr *op_desc,
  ::ge::ComputeGraphPtr *ge_graph) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*op_desc);
  auto depends_list_me = abstract::GetValueDependArgIndices(node);
  if (depends_list_me.empty() || AnfAlgo::IsDynamicShapeSkipExecute(node)) {
    MS_LOG(INFO) << "The node " << op_name_ << " has no infer depend.";
    return {};
  }
  auto has_input_name_attr = common::AnfAlgo::HasNodeAttr("input_names", node);
  if (!has_input_name_attr) {
    MS_LOG(EXCEPTION) << "Node should has attr: input_names. " << node->fullname_with_scope();
  }

  auto input_names_attr = common::AnfAlgo::GetNodeAttr<std::vector<std::string>>(node, "input_names");
  std::vector<std::string> op_infer_depends;
  std::vector<std::tuple<std::size_t, ::ge::NodePtr>> constant_ops;
  for (auto index : depends_list_me) {
    if (LongToSize(index) > input_names_attr.size()) {
      MS_LOG(EXCEPTION) << "Input index " << index << " should not be greater than input_names' size "
                        << input_names_attr.size();
    }
    auto iter = depend_tensor_map.find(LongToSize(index));
    if (iter == depend_tensor_map.end()) {
      MS_LOG(EXCEPTION) << "Input index " << index << " should be less than depend_tensor_map' size "
                        << input_names_attr.size();
    }
    auto depend_name = input_names_attr[index];
    auto const_tensor = iter->second;
    ::ge::NodePtr ge_constant_op = NewConstantOp(node, depend_name, const_tensor, ge_graph, index);
    auto original_index = AnfAlgo::GetInputIndexInKernel(node, index);
    constant_ops.emplace_back(std::tuple<std::size_t, ::ge::NodePtr>(original_index, ge_constant_op));
    op_infer_depends.emplace_back(depend_name);
  }
  (void)(*op_desc)->SetOpInferDepends(op_infer_depends);
  return constant_ops;
}

void OpTilingCalculateAdapter::AddEdge(const ::ge::NodePtr &ge_node,
                                       const std::vector<std::tuple<std::size_t, ::ge::NodePtr>> &constant_ops) {
  MS_EXCEPTION_IF_NULL(ge_node);
  MS_LOG(INFO) << "Add edge for op " << op_name_;
  for (const auto &item : constant_ops) {
    auto index = std::get<0>(item);
    auto constant_op = std::get<1>(item);
    (void)ge_node->AddLinkFrom(index, constant_op);
  }
}

void OpTilingCalculateAdapter::InitOpIoName(const CNodePtr &node) {
  MS_EXCEPTION_IF_NULL(node);
  MS_LOG(INFO) << "Get the every input name of " << op_name_;
  auto op_info_ptr = mindspore::kernel::tbe::TbeDynamicShapeUtil::FindOp(op_name_, node);
  MS_EXCEPTION_IF_NULL(op_info_ptr);
  auto primitive = common::AnfAlgo::GetCNodePrimitive(node);
  MS_EXCEPTION_IF_NULL(primitive);
  auto inputs_ptr = op_info_ptr->inputs_ptr();
  size_t dynamic_input_index = 0;
  std::vector<int64_t> dynamic_inputs_list;
  if (primitive->GetAttr(kAttrDynInputSizes) != nullptr) {
    dynamic_inputs_list = GetValue<std::vector<int64_t>>(primitive->GetAttr(kAttrDynInputSizes));
  }
  for (const auto &item : inputs_ptr) {
    MS_EXCEPTION_IF_NULL(item);
    if (item->param_type() == PARAM_DYNAMIC) {
      if (dynamic_input_index > dynamic_inputs_list.size()) {
        MS_LOG(EXCEPTION) << "Dynamic input index should be less than the dynamic input's size.";
      }
      auto real_inputs_num = dynamic_inputs_list[dynamic_input_index];
      for (auto k = 0; k < real_inputs_num; k++) {
        std::string input_name = item->name() + "_dynamic_" + std::to_string(k);
        input_names_.emplace_back(input_name);
      }
      dynamic_input_index++;
    } else {
      input_names_.emplace_back(item->name());
    }
  }

  // output io names
  auto outputs_ptr = op_info_ptr->outputs_ptr();
  for (const auto &out_item : outputs_ptr) {
    MS_EXCEPTION_IF_NULL(out_item);
    if (out_item->param_type() == PARAM_DYNAMIC && outputs_ptr.size() == 1) {
      std::string output_name;
      auto real_outputs_size = common::AnfAlgo::GetOutputTensorNum(node);
      for (size_t i = 0; i < real_outputs_size; i++) {
        output_name = out_item->name() + "_dynamic_" + std::to_string(i);
        output_names_.emplace_back(output_name);
      }
    } else {
      output_names_.emplace_back(out_item->name());
    }
  }
}

::ge::NodePtr OpTilingCalculateAdapter::CreateGeNode(const CNodePtr &node, ::ge::ComputeGraphPtr *ge_graph,
                                                     const std::map<uint32_t, tensor::TensorPtr> &depend_tensor_map,
                                                     const std::string &op_compile_info) {
  MS_EXCEPTION_IF_NULL(node);
  op_name_ = common::AnfAlgo::GetCNodeName(node);
  MS_LOG(INFO) << "Convert anf node :" << op_name_ << " to ge node.";
  op_compile_info_ = op_compile_info;
  auto op_type = GetRealOpType(op_name_);
  (void)InitOpIoName(node);
  ::ge::OpDescPtr op_desc = std::make_shared<::ge::OpDesc>(op_name_, op_type);
  MS_EXCEPTION_IF_NULL(op_desc);
  ConvertInputShapeAndType(node, &op_desc);
  ConvertOutputShapeAndType(node, &op_desc);
  ConvertAttrs(node, &op_desc);
  ConvertCompileInfo(node, &op_desc);
  ConvertAtomicCompileInfo(node, &op_desc);
  std::vector<std::tuple<std::size_t, ::ge::NodePtr>> constant_ops =
    ConvertDepends(node, depend_tensor_map, &op_desc, ge_graph);
  MS_EXCEPTION_IF_NULL(*ge_graph);
  auto ge_node = (*ge_graph)->AddNode(op_desc);
  AddEdge(ge_node, constant_ops);
  return ge_node;
}

::ge::Operator OpTilingCalculateAdapter::GeNodeToGeOperatorAdapter(const ::ge::NodePtr &ge_node) const {
  MS_EXCEPTION_IF_NULL(ge_node);
  return ::ge::OpDescUtils::CreateOperatorFromNode(ge_node);
}

::ge::NodePtr OpTilingCalculateAdapter::AnfNodeToGeNodeAdapter(
  const CNodePtr &node, ::ge::ComputeGraphPtr *ge_graph, const std::map<uint32_t, tensor::TensorPtr> &depend_tensor_map,
  const std::string &op_compile_info) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*ge_graph);
  return CreateGeNode(node, ge_graph, depend_tensor_map, op_compile_info);
}

::ge::Operator OpTilingCalculateAdapter::AnfNodeToGeOperatorAdapter(
  const CNodePtr &node, ::ge::ComputeGraphPtr *ge_graph, const std::map<uint32_t, tensor::TensorPtr> &depend_tensor_map,
  const std::string &op_compile_info) {
  MS_EXCEPTION_IF_NULL(node);
  MS_EXCEPTION_IF_NULL(*ge_graph);
  auto ge_node = CreateGeNode(node, ge_graph, depend_tensor_map, op_compile_info);
  MS_EXCEPTION_IF_NULL(ge_node);
  return ::ge::OpDescUtils::CreateOperatorFromNode(ge_node);
}
}  // namespace tiling
}  // namespace device
}  // namespace mindspore
