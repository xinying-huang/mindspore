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

#ifndef MINDSPORE_CORE_IR_DTYPE_CONTAINER_H_
#define MINDSPORE_CORE_IR_DTYPE_CONTAINER_H_

#include <cstddef>
#include <iostream>
#include <initializer_list>
#include <map>
#include <memory>
#include <utility>
#include <sstream>
#include <string>
#include <vector>
#include <type_traits>
#include <algorithm>

#include "utils/hash_map.h"
#include "base/base.h"
#include "ir/named.h"
#include "ir/dtype/type.h"

namespace mindspore {
/// \brief List defines interface for list data type.
class MS_CORE_API List final : public Object {
 public:
  /// \brief Default constructor for List.
  List() : Object(kObjectTypeList) {}

  /// \brief Constructor for List.
  ///
  /// \param[in] objs The elements of List.
  List(const std::initializer_list<TypePtr> &objs)
      : Object(kObjectTypeList, false), elements_(objs.begin(), objs.end()) {}

  /// \brief Shadow copy function for List.
  ///
  /// \param[in] obj TypePtrList to be copied.
  explicit List(const TypePtrList &obj) : Object(kObjectTypeList, false), elements_(obj) {}

  /// \brief Destructor of List.
  ~List() override {}
  MS_DECLARE_PARENT(List, Object)

  /// \brief Get type of List element.
  ///
  /// \param[in] dim Define the index of List element.
  /// \return TypePtr of List element.
  const TypePtr operator[](std::size_t dim) const;

  TypeId generic_type_id() const override { return kObjectTypeList; }
  TypePtr DeepCopy() const override;
  bool operator==(const Type &other) const override;

  /// \brief Get the number of elements in this List.
  ///
  /// \return The number of elements in this List.
  std::size_t size() const { return elements_.size(); }

  /// \brief Get the elements of List object.
  ///
  /// \return The elements of List object.
  TypePtrList elements() const { return elements_; }
  std::string ToReprString() const override { return "list_"; }
  std::string ToString() const override { return DumpContent(false); }
  std::string DumpText() const override { return DumpContent(true); };

 private:
  /// \brief Show each element.
  ///
  /// \param[in] is_dumptext whether to show each element DumpText
  /// \return The description of the List object.
  std::string DumpContent(bool is_dumptext) const;
  TypePtrList elements_;
};
using ListPtr = std::shared_ptr<List>;

/// \brief Tuple defines interface for tuple data type.
class MS_CORE_API Tuple final : public Object {
 public:
  /// \brief Default constructor for Tuple.
  Tuple() : Object(kObjectTypeTuple) {}

  /// \brief Constructor for Tuple.
  ///
  /// \param[in] objs The elements of Tuple.
  Tuple(const std::initializer_list<TypePtr> &objs)
      : Object(kObjectTypeTuple, false), elements_(objs.begin(), objs.end()) {}

  /// \brief Shadow copy function for Tuple.
  ///
  /// \param[in] objs TypePtrList to be copied.
  explicit Tuple(const TypePtrList &objs) : Object(kObjectTypeTuple, false), elements_(objs.begin(), objs.end()) {}

  /// \brief Destructor of Tuple.
  ~Tuple() override {}
  MS_DECLARE_PARENT(Tuple, Object)

  TypeId generic_type_id() const override { return kObjectTypeTuple; }
  TypePtr DeepCopy() const override;
  std::string ToReprString() const override { return "tuple_"; }
  std::string ToString() const override { return DumpContent(false); }
  std::string DumpText() const override { return DumpContent(true); }

  /// \brief Get type of Tuple element.
  ///
  /// \param[in] dim Define the index of Tuple element.
  /// \return TypePtr of Tuple element.
  const TypePtr operator[](std::size_t dim) const;

  bool operator==(const Type &other) const override;

  /// \brief Get the elements of the Tuple object.
  ///
  /// \return The elements of the Tuple object.
  TypePtrList elements() const { return elements_; }

  /// \brief Get the number of elements in the Tuple object.
  ///
  /// \return The number of elements in the Tuple object.
  std::size_t size() const { return elements_.size(); }

 private:
  /// \brief Show each element.
  ///
  /// \param[in] is_dumptext whether to show each element DumpText
  /// \return The description of the Tuple object.
  std::string DumpContent(bool is_dumptext) const;
  TypePtrList elements_;
};
using TuplePtr = std::shared_ptr<Tuple>;

/// \brief Dictionary defines interface for dictionary data type.
class MS_CORE_API Dictionary final : public Object {
 public:
  /// \brief Default constructor for Dictionary.
  Dictionary() : Object(kObjectTypeDictionary) {}

  /// \brief Constructor for Dictionary.
  ///
  /// \param[in] key_values The elements of Dictionary.
  explicit Dictionary(const std::vector<std::pair<std::string, TypePtr>> &key_values)
      : Object(kObjectTypeDictionary, false), key_values_(key_values) {}

  /// \brief Destructor of Dictionary.
  ~Dictionary() override {}
  MS_DECLARE_PARENT(Dictionary, Object)

  TypeId generic_type_id() const override { return kObjectTypeDictionary; }
  bool operator==(const Type &other) const override;
  TypePtr DeepCopy() const override;
  std::string ToString() const override { return DumpContent(false); }
  std::string DumpText() const override { return DumpContent(true); }

 private:
  /// \brief Show each element.
  ///
  /// \param[in] is_dumptext whether to show each element DumpText
  /// \return The description of the Dictionary object.
  std::string DumpContent(bool) const;
  std::vector<std::pair<std::string, TypePtr>> key_values_;
};
using DictionaryPtr = std::shared_ptr<Dictionary>;
}  // namespace mindspore

#endif  // MINDSPORE_CORE_IR_DTYPE_CONTAINER_H_
