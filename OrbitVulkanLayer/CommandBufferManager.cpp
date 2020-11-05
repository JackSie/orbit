// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CommandBufferManager.h"

#include <sys/syscall.h>
#include <unistd.h>

#include "OrbitBase/Logging.h"
#define gettid() syscall(SYS_gettid)

namespace orbit_vulkan_layer {

void CommandBufferManager::TrackCommandBuffers(VkDevice device, VkCommandPool pool,
                                               const VkCommandBuffer* command_buffers,
                                               uint32_t count) {
  LOG("TrackCommandBuffers");
  absl::WriterMutexLock lock(&mutex_);
  if (!pool_to_command_buffers_.contains(pool)) {
    pool_to_command_buffers_[pool] = {};
  }
  absl::flat_hash_set<VkCommandBuffer>& associated_cbs = pool_to_command_buffers_.at(pool);
  for (uint32_t i = 0; i < count; ++i) {
    const VkCommandBuffer& cb = command_buffers[i];
    CHECK(cb != VK_NULL_HANDLE);
    associated_cbs.insert(cb);
    command_buffer_to_device_[cb] = device;
  }
}

void CommandBufferManager::UntrackCommandBuffers(VkDevice device, VkCommandPool pool,
                                                 const VkCommandBuffer* command_buffers,
                                                 uint32_t count) {
  LOG("UntrackCommandBuffers");
  absl::WriterMutexLock lock(&mutex_);
  absl::flat_hash_set<VkCommandBuffer>& associated_command_buffers =
      pool_to_command_buffers_.at(pool);
  for (uint32_t i = 0; i < count; ++i) {
    const VkCommandBuffer& command_buffer = command_buffers[i];
    CHECK(command_buffer != VK_NULL_HANDLE);
    associated_command_buffers.erase(command_buffer);
    CHECK(command_buffer_to_device_.contains(command_buffer));
    CHECK(command_buffer_to_device_.at(command_buffer) == device);
    command_buffer_to_device_.erase(command_buffer);
  }
  if (associated_command_buffers.empty()) {
    pool_to_command_buffers_.erase(pool);
  }
}

void CommandBufferManager::MarkCommandBufferBegin(const VkCommandBuffer& command_buffer) {
  LOG("MarkCommandBufferBegin");
  if (!connector_->IsCapturing()) {
    return;
  }
  VkDevice device;
  {
    absl::ReaderMutexLock lock(&mutex_);
    CHECK(command_buffer_to_device_.contains(command_buffer));
    device = command_buffer_to_device_.at(command_buffer);
  }

  VkQueryPool query_pool = timer_query_pool_->GetQueryPool(device);

  // Let's try to reset the slots from the completed submits:
  std::vector<uint32_t> base_slots_to_reset;
  if (timer_query_pool_->IsResetNeeded()) {
    base_slots_to_reset = timer_query_pool_->PullSlotsToReset(device);
    CHECK(!base_slots_to_reset.empty());
    for (uint32_t base_slot_to_reset : base_slots_to_reset) {
      dispatch_table_->CmdResetQueryPool(command_buffer)(command_buffer, query_pool,
                                                         base_slot_to_reset, 2);
    }
  }

  uint32_t slot_index;
  bool found_slot = timer_query_pool_->NextReadyQuerySlot(device, &slot_index);
  CHECK(found_slot);
  dispatch_table_->CmdWriteTimestamp(command_buffer)(
      command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, slot_index);
  {
    absl::WriterMutexLock lock(&mutex_);
    CHECK(!command_buffer_to_state_.contains(command_buffer));
    MarkerState marker_state{
        .type = kCommandBuffer, .text = "Command Buffer", .slot_index = slot_index};
    command_buffer_to_state_[command_buffer] = {
        .command_buffer_marker = std::move(marker_state),
        .resetting_slot_indices = std::move(base_slots_to_reset)};
  }
}

void CommandBufferManager::MarkCommandBufferEnd(const VkCommandBuffer& command_buffer) {
  LOG("MarkCommandBufferEnd");
  if (!connector_->IsCapturing()) {
    return;
  }
  absl::ReaderMutexLock lock(&mutex_);
  if (!command_buffer_to_state_.contains(command_buffer)) {
    return;
  }
  VkDevice device;
  {
    CHECK(command_buffer_to_device_.contains(command_buffer));
    device = command_buffer_to_device_.at(command_buffer);
  }
  VkQueryPool query_pool = timer_query_pool_->GetQueryPool(device);

  CommandBufferState& command_buffer_state = command_buffer_to_state_.at(command_buffer);

  uint32_t slot_base_index = command_buffer_state.command_buffer_marker.slot_index;

  dispatch_table_->CmdWriteTimestamp(command_buffer)(
      command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, slot_base_index + 1);
}

void CommandBufferManager::DoSubmit(VkQueue queue, uint32_t submit_count,
                                    const VkSubmitInfo* submits) {
  LOG("DoSubmit");
  if (!connector_->IsCapturing()) {
    return;
  }
  absl::WriterMutexLock lock(&mutex_);
  if (!submitted_queues_.contains(queue)) {
    submitted_queues_[queue] = {};
  }
  SubmittedQueue& submitted_queue = submitted_queues_.at(queue);
  for (uint32_t submit_index = 0; submit_index < submit_count; ++submit_index) {
    const VkSubmitInfo& submit_info = submits[submit_index];
    submitted_queue.submitted_submit_infos.emplace_back();
    SubmittedSubmitInfo& submitted_submit_info = submitted_queue.submitted_submit_infos.back();
    for (uint32_t command_buffer_index = 0; command_buffer_index < submit_info.commandBufferCount;
         ++command_buffer_index) {
      const VkCommandBuffer& command_buffer = submit_info.pCommandBuffers[command_buffer_index];
      if (!command_buffer_to_state_.contains(command_buffer)) {
        continue;
      }
      CommandBufferState& state = command_buffer_to_state_.at(command_buffer);
      SubmittedCommandBuffer submitted_command_buffer{
          .command_buffer_marker = std::move(state.command_buffer_marker),
          .resetting_slot_indices = std::move(state.resetting_slot_indices)};
      submitted_submit_info.command_buffers.emplace_back(std::move(submitted_command_buffer));
      command_buffer_to_state_.erase(command_buffer);
    }
  }
}

void CommandBufferManager::CompleteSubmits(const VkDevice& device) {
  LOG("CompleteSubmits");
  VkQueryPool query_pool = timer_query_pool_->GetQueryPool(device);
  std::vector<SubmittedSubmitInfo> completed_submits;
  {
    absl::WriterMutexLock lock(&mutex_);
    for (auto& [_, submitted_queue] : submitted_queues_) {
      auto submit_iter = submitted_queue.submitted_submit_infos.begin();
      while (submit_iter != submitted_queue.submitted_submit_infos.end()) {
        const SubmittedSubmitInfo& submit_info = *submit_iter;
        if (submit_info.command_buffers.empty()) {
          submit_iter = submitted_queue.submitted_submit_infos.erase(submit_iter);
          continue;
        }
        uint32_t check_slot_index_base =
            submit_info.command_buffers.back().command_buffer_marker.slot_index;

        VkDeviceSize result_stride = sizeof(uint64_t);
        uint64_t test_query_result = 0;
        VkResult query_worked = dispatch_table_->GetQueryPoolResults(device)(
            device, query_pool, check_slot_index_base + 1, 1, sizeof(test_query_result),
            &test_query_result, result_stride, VK_QUERY_RESULT_64_BIT);
        if (query_worked == VK_SUCCESS) {
          completed_submits.emplace_back(submit_info);
          submit_iter = submitted_queue.submitted_submit_infos.erase(submit_iter);
        } else {
          submit_iter++;
        }
      }
    }
  }

  if (completed_submits.empty()) {
    return;
  }

  const VkPhysicalDevice& physical_device =
      physical_device_manager_->GetPhysicalDeviceOfLogicalDevice(device);
  const float timestamp_period =
      physical_device_manager_->GetPhysicalDeviceProperties(physical_device).limits.timestampPeriod;

  std::vector<uint32_t> query_slots_to_reset;
  std::vector<uint32_t> query_slots_reset_ready;
  for (const auto& completed_submit : completed_submits) {
    for (const auto& completed_command_buffer : completed_submit.command_buffers) {
      query_slots_reset_ready.insert(query_slots_reset_ready.end(),
                                     completed_command_buffer.resetting_slot_indices.begin(),
                                     completed_command_buffer.resetting_slot_indices.end());
      const MarkerState& marker = completed_command_buffer.command_buffer_marker;
      VkDeviceSize result_stride = sizeof(uint64_t);

      uint64_t begin_timestamp = 0;
      VkResult result_status = dispatch_table_->GetQueryPoolResults(device)(
          device, query_pool, marker.slot_index, 1, sizeof(begin_timestamp), &begin_timestamp,
          result_stride, VK_QUERY_RESULT_64_BIT);
      CHECK(result_status == VK_SUCCESS);

      uint64_t end_timestamp = 0;
      result_status = dispatch_table_->GetQueryPoolResults(device)(
          device, query_pool, marker.slot_index + 1, 1, sizeof(end_timestamp), &end_timestamp,
          result_stride, VK_QUERY_RESULT_64_BIT);

      CHECK(result_status == VK_SUCCESS);

      begin_timestamp =
          static_cast<uint64_t>(static_cast<double>(begin_timestamp) * timestamp_period);
      end_timestamp = static_cast<uint64_t>(static_cast<double>(end_timestamp) * timestamp_period);

      int32_t tid = gettid();

      writer_->WriteCommandBuffer(
          begin_timestamp, end_timestamp,
          physical_device_manager_->GetApproxCpuTimestampOffset(physical_device), tid);

      query_slots_to_reset.push_back(marker.slot_index);
    }
  }

  timer_query_pool_->MarkSlotsReadyForReset(device, query_slots_to_reset);
  timer_query_pool_->MarkSlotsReadyForQuery(device, query_slots_reset_ready);
}

void CommandBufferManager::ResetCommandBuffer(const VkCommandBuffer& command_buffer) {
  LOG("ResetCommandBuffer");
  absl::WriterMutexLock lock(&mutex_);
  if (!command_buffer_to_state_.contains(command_buffer)) {
    return;
  }
  CommandBufferState& state = command_buffer_to_state_.at(command_buffer);
  const VkDevice& device = command_buffer_to_device_.at(command_buffer);
  std::vector<uint32_t> marker_slots_to_rollback;
  marker_slots_to_rollback.push_back(state.command_buffer_marker.slot_index);
  timer_query_pool_->RollbackPendingQuerySlots(device, marker_slots_to_rollback);
  timer_query_pool_->RollbackPendingResetSlots(device, state.resetting_slot_indices);

  command_buffer_to_state_.erase(command_buffer);
}

void CommandBufferManager::ResetCommandPool(const VkCommandPool& command_pool) {
  LOG("ResetCommandPool");
  absl::flat_hash_set<VkCommandBuffer> command_buffers;
  {
    absl::ReaderMutexLock lock(&mutex_);
    if (!pool_to_command_buffers_.contains(command_pool)) {
      return;
    }
    command_buffers = pool_to_command_buffers_.at(command_pool);
  }
  for (const auto& command_buffer : command_buffers) {
    ResetCommandBuffer(command_buffer);
  }
}

}  // namespace orbit_vulkan_layer