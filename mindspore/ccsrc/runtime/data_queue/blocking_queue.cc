/**
 * Copyright 2019 Huawei Technologies Co., Ltd
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

#include "runtime/data_queue/blocking_queue.h"
#include "utils/log_adapter.h"

namespace mindspore {
namespace device {
const size_t kTimeout = 100;
void BlockingQueue::RegisterRelease(const std::function<void(void *, int32_t)> &func) { queue_->RegisterRelease(func); }

BlockQueueStatus_T BlockingQueue::Push(const std::vector<DataQueueItem> &data, unsigned int) {
  std::unique_lock<std::mutex> locker(mutex_);
  if (queue_->IsFull()) {
    if (not_full_cond_.wait_for(locker, std::chrono::microseconds(kTimeout)) == std::cv_status::timeout) {
      return TIMEOUT;
    }
  }
  auto ret = queue_->Push(data);
  if (ret != SUCCESS) {
    return ret;
  }
  not_empty_cond_.notify_one();
  return SUCCESS;
}

BlockQueueStatus_T BlockingQueue::Front(std::vector<DataQueueItem> *data) {
  std::unique_lock<std::mutex> locker(mutex_);
  bool timeout = not_empty_cond_.wait_for(locker, std::chrono::seconds(30), [this] { return !queue_->IsEmpty(); });
  if (!timeout) {
    return TIMEOUT;
  }
  return queue_->Front(data);
}

BlockQueueStatus_T BlockingQueue::Pop() {
  std::unique_lock<std::mutex> locker(mutex_);
  not_empty_cond_.wait(locker, [this] { return !queue_->IsEmpty(); });
  auto ret = queue_->Pop();
  if (ret != SUCCESS) {
    return ret;
  }
  not_full_cond_.notify_one();
  return SUCCESS;
}

BlockQueueStatus_T BlockingQueue::Create(const std::shared_ptr<DataQueue> &data_queue) {
  this->queue_ = data_queue;
  return SUCCESS;
}

BlockQueueStatus_T BlockingQueue::Clear() {
  std::unique_lock<std::mutex> locker(mutex_);
  while (Size() > 0) {
    std::vector<DataQueueItem> data;
    auto ret = queue_->Front(&data);
    if (ret != SUCCESS) {
      return ret;
    }
    ret = queue_->Pop();
    if (ret != SUCCESS) {
      return ret;
    }
  }
  return SUCCESS;
}

bool BlockingQueue::Destroy() {
  if (queue_ != nullptr) {
    return queue_->Destroy();
  } else {
    return true;
  }
}
}  // namespace device
}  // namespace mindspore
