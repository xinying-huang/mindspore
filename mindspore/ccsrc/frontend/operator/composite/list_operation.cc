/**
 * Copyright 2019-2022 Huawei Technologies Co., Ltd
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

#include "frontend/operator/composite/list_operation.h"

#include <vector>
#include <string>
#include <memory>

#include "abstract/param_validator.h"
#include "frontend/optimizer/opt.h"
#include "include/common/pybind_api/api_register.h"

namespace mindspore {
// namespace to support composite operators definition
namespace prim {
FuncGraphPtr ListAppend::GenerateFuncGraph(const abstract::AbstractBasePtrList &args_list) {
  abstract::CheckArgsSize("ListAppend", args_list, 2);

  AbstractBasePtr arg0 = args_list[0];
  abstract::AbstractListPtr arg0_list = dyn_cast<abstract::AbstractList>(arg0);
  MS_EXCEPTION_IF_NULL(arg0_list);

  FuncGraphPtr ret = std::make_shared<FuncGraph>();
  ret->set_flag(FUNC_GRAPH_FLAG_CORE, true);
  ret->debug_info()->set_name("append");
  AnfNodePtr arg0_node = ret->add_parameter();

  std::vector<AnfNodePtr> elems;
  elems.push_back(NewValueNode(prim::kPrimMakeList));
  size_t arg0_length = arg0_list->size();
  for (size_t i = 0; i < arg0_length; ++i) {
    elems.push_back(ret->NewCNode({NewValueNode(prim::kPrimListGetItem), arg0_node, NewValueNode(SizeToLong(i))}));
  }
  AnfNodePtr arg1_node = ret->add_parameter();
  elems.push_back(arg1_node);

  ret->set_output(ret->NewCNode(elems));
  return ret;
}

FuncGraphPtr ListInsert::GenerateFuncGraph(const abstract::AbstractBasePtrList &args_list) {
  const size_t list_insert_args_size = 3;
  abstract::CheckArgsSize("ListInsert", args_list, list_insert_args_size);
  AbstractBasePtr arg0 = args_list[0];
  AbstractBasePtr arg1 = args_list[1];

  abstract::AbstractListPtr arg0_list = dyn_cast<abstract::AbstractList>(arg0);
  MS_EXCEPTION_IF_NULL(arg0_list);
  size_t list_len = arg0_list->size();
  int64_t len = SizeToLong(list_len);
  FuncGraphPtr ret = std::make_shared<FuncGraph>();
  ret->set_flag(FUNC_GRAPH_FLAG_CORE, true);
  ret->debug_info()->set_name("insert");
  AnfNodePtr arg0_node = ret->add_parameter();
  (void)ret->add_parameter();
  AnfNodePtr arg2_node = ret->add_parameter();

  std::vector<AnfNodePtr> elems;
  elems.push_back(NewValueNode(prim::kPrimMakeList));
  int64_t index_value = GetValue<int64_t>(arg1->BuildValue());
  int64_t insert_position = 0;
  if (index_value >= len) {
    insert_position = len;
  } else if (index_value > 0 && index_value < len) {
    insert_position = index_value;
  } else if (index_value < 0 && index_value > -len) {
    insert_position = len + index_value;
  }
  for (int64_t i = 0; i < insert_position; ++i) {
    auto value = ret->NewCNode({NewValueNode(prim::kPrimListGetItem), arg0_node, NewValueNode(i)});
    elems.push_back(value);
  }
  elems.push_back(arg2_node);
  for (int64_t i = insert_position; i < len; ++i) {
    auto value = ret->NewCNode({NewValueNode(prim::kPrimListGetItem), arg0_node, NewValueNode(i)});
    elems.push_back(value);
  }
  auto out = ret->NewCNode(elems);
  ret->set_output(out);
  return ret;
}

FuncGraphPtr ListPop::GenerateFuncGraph(const abstract::AbstractBasePtrList &args_list) {
  constexpr size_t list_pop_args_size = 2;
  abstract::CheckArgsSize("ListPop", args_list, list_pop_args_size);
  abstract::AbstractListPtr list_input = dyn_cast<abstract::AbstractList>(args_list[0]);
  AbstractBasePtr pop_index = args_list[1];
  MS_EXCEPTION_IF_NULL(list_input);
  size_t list_len = list_input->size();
  int64_t len = SizeToLong(list_len);
  FuncGraphPtr ret = std::make_shared<FuncGraph>();
  ret->set_flag(FUNC_GRAPH_FLAG_CORE, true);
  ret->debug_info()->set_name("pop");
  AnfNodePtr arg0_node = ret->add_parameter();
  (void)ret->add_parameter();

  std::vector<AnfNodePtr> elems;
  elems.push_back(NewValueNode(prim::kPrimMakeList));
  int64_t index_value = GetValue<int64_t>(pop_index->BuildValue());
  if (index_value >= len || index_value < -1 * len) {
    MS_EXCEPTION(IndexError) << "The pop index out of range.";
  }
  int64_t pop_position = (index_value >= 0) ? index_value : (len + index_value);

  for (int64_t i = 0; i < pop_position; ++i) {
    auto value = ret->NewCNode({NewValueNode(prim::kPrimListGetItem), arg0_node, NewValueNode(i)});
    elems.push_back(value);
  }
  auto pop_node = ret->NewCNode({NewValueNode(prim::kPrimListGetItem), arg0_node, NewValueNode(pop_position)});
  for (int64_t i = pop_position + 1; i < len; ++i) {
    auto value = ret->NewCNode({NewValueNode(prim::kPrimListGetItem), arg0_node, NewValueNode(i)});
    elems.push_back(value);
  }

  auto new_list = ret->NewCNode(elems);
  auto out = ret->NewCNode({NewValueNode(prim::kPrimMakeTuple), new_list, pop_node});
  ret->set_output(out);
  return ret;
}
}  // namespace prim
}  // namespace mindspore
