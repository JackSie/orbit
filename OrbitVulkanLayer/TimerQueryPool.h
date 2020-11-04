// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_VULKAN_LAYER_TIMER_QUERY_POOL_H_
#define ORBIT_VULKAN_LAYER_TIMER_QUERY_POOL_H_

#include "DispatchTable.h"
#include "OrbitBase/Logging.h"
#include "QueueFamilyInfoManager.h"
#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"

namespace orbit_vulkan_layer {

/*
 * We use each "logical" query slot twice, once for "Begin" queries and once for "end" queries.
 * Thus, we have two times the number of "physical".
 * This assumes, that the slot state of the physical slots is always the same for begin and ends,
 * which needs to be ensured by the caller.
 *
 * To translate a logical slot to a physical begin slot: logical_slot * 2
 * To translate a logical slot to a physical end slot: logical_slot * 2 + 1
 */
class TimerQueryPool {
 public:
  explicit TimerQueryPool(DispatchTable* dispatch_table,
                          QueueFamilyInfoManager* queue_family_info_manager,
                          PhysicalDeviceManager* physical_device_manager)
      : dispatch_table_(dispatch_table),
        queue_family_info_manager_(queue_family_info_manager),
        physical_device_manager_(physical_device_manager) {}
  void InitializeTimerQueryPool(const VkDevice& device, const VkPhysicalDevice& physical_device);
  [[nodiscard]] VkQueryPool GetQueryPool(const VkDevice& device);
  [[nodiscard]] bool NextReadyQuerySlot(const VkDevice& device, uint32_t* allocated_index);

  void MarkSlotsReadyForReset(const VkDevice& device,
                              const std::vector<uint32_t>& physical_slot_indices) {
    if (physical_slot_indices.empty()) {
      return;
    }
    LOG("MarkSlotsReadyForReset");
    reset_needed_ = true;
    absl::WriterMutexLock lock(&mutex_);
    CHECK(device_to_query_slots_.contains(device));
    std::array<SlotState, kNumLogicalQuerySlots>& slot_states = device_to_query_slots_.at(device);
    for (uint32_t physical_slot_index : physical_slot_indices) {
      CHECK(physical_slot_index < kNumPhysicalTimerQuerySlots);
      bool is_base_slot = (physical_slot_index % 2) == 0;
      CHECK(is_base_slot);
      uint32_t logical_slot_index = physical_slot_index / 2;
      CHECK(slot_states.size() > logical_slot_index);
      const SlotState& current_state = slot_states.at(logical_slot_index);
      CHECK(current_state == kQueryPendingOnGPU);
      slot_states.at(logical_slot_index) = kReadyForResetIssue;
      CHECK(device_to_pending_reset_slots_.contains(device));
      device_to_pending_reset_slots_.at(device).push_back(physical_slot_index);
    }
  }

  void MarkSlotsReadyForQuery(const VkDevice& device,
                              const std::vector<uint32_t>& physical_slot_indices) {
    if (physical_slot_indices.empty()) {
      return;
    }
    LOG("MarkSlotsReadyForQuery");
    absl::WriterMutexLock lock(&mutex_);
    std::array<SlotState, kNumLogicalQuerySlots>& slot_states = device_to_query_slots_.at(device);
    for (uint32_t physical_slot_index : physical_slot_indices) {
      CHECK(physical_slot_index < kNumPhysicalTimerQuerySlots);
      bool is_base_slot = (physical_slot_index % 2) == 0;
      CHECK(is_base_slot);
      uint32_t logical_slot_index = physical_slot_index / 2;
      const SlotState& current_state = slot_states.at(logical_slot_index);
      CHECK(current_state == kResetPendingOnGPU);
      slot_states.at(logical_slot_index) = kReadyForQueryIssue;
    }
  }

  void RollbackPendingQuerySlots(const VkDevice& device,
                                 const std::vector<uint32_t>& physical_slot_indices) {
    if (physical_slot_indices.empty()) {
      return;
    }
    LOG("RollbackPendingQuerySlots");
    absl::WriterMutexLock lock(&mutex_);
    std::array<SlotState, kNumLogicalQuerySlots>& slot_states = device_to_query_slots_.at(device);
    for (uint32_t physical_slot_index : physical_slot_indices) {
      CHECK(physical_slot_index < kNumPhysicalTimerQuerySlots);
      bool is_base_slot = (physical_slot_index % 2) == 0;
      CHECK(is_base_slot);
      uint32_t logical_slot_index = physical_slot_index / 2;
      const SlotState& current_state = slot_states.at(logical_slot_index);
      if (current_state != kQueryPendingOnGPU) {
        LOG("state: %u, index: %u", current_state, logical_slot_index);
      }
      CHECK(current_state == kQueryPendingOnGPU);
      slot_states.at(logical_slot_index) = kReadyForQueryIssue;
    }
  }

  void RollbackPendingResetSlots(const VkDevice& device,
                                 const std::vector<uint32_t>& physical_slot_indices) {
    if (physical_slot_indices.empty()) {
      return;
    }
    LOG("RollbackPendingResetSlots");
    reset_needed_ = true;
    absl::WriterMutexLock lock(&mutex_);
    std::array<SlotState, kNumLogicalQuerySlots>& slot_states = device_to_query_slots_.at(device);
    std::vector<uint32_t>& pending_reset_slots = device_to_pending_reset_slots_.at(device);
    for (uint32_t physical_slot_index : physical_slot_indices) {
      CHECK(physical_slot_index < kNumPhysicalTimerQuerySlots);
      bool is_base_slot = (physical_slot_index % 2) == 0;
      CHECK(is_base_slot);
      uint32_t logical_slot_index = physical_slot_index / 2;
      const SlotState& current_state = slot_states.at(logical_slot_index);
      CHECK(current_state == kResetPendingOnGPU);
      slot_states.at(logical_slot_index) = kReadyForResetIssue;
      pending_reset_slots.push_back(physical_slot_index);
    }
  }

  [[nodiscard]] bool IsResetNeeded() {
    LOG("IsResetNeeded");
    return reset_needed_;
  }

  [[nodiscard]] std::vector<uint32_t> PullSlotsToReset(const VkDevice& device) {
    LOG("PullSlotsToReset");
    // We want to avoid lock-contention, so we assume the caller has called IsResetNeeded() before
    // and to be sure, that there was no Pull in between, test again.
    bool true_value = true;
    if (!reset_needed_.compare_exchange_strong(true_value, false)) {
      return {};
    }
    absl::WriterMutexLock lock(&mutex_);
    std::vector<uint32_t> pending_reset_slots =
        std::move(device_to_pending_reset_slots_.at(device));
    auto& slot_states = device_to_query_slots_.at(device);
    for (uint32_t physical_slot_index : pending_reset_slots) {
      bool is_base_slot = physical_slot_index % 2 == 0;
      CHECK(is_base_slot);
      uint32_t logical_slot_index = physical_slot_index / 2;
      CHECK(slot_states.at(logical_slot_index) == kReadyForResetIssue);
      slot_states.at(logical_slot_index) = kResetPendingOnGPU;
    }
    device_to_pending_reset_slots_.at(device).clear();
    return pending_reset_slots;
  }

  void PrintState(const VkDevice& device) {
    absl::ReaderMutexLock lock(&mutex_);
    uint32_t ready_for_query = 0;
    uint32_t query_pending = 0;
    uint32_t ready_for_reset = 0;
    uint32_t reset_pending = 0;
    for (const SlotState& slot_state : device_to_query_slots_.at(device)) {
      if (slot_state == kReadyForQueryIssue) {
        ready_for_query++;
        continue;
      }
      if (slot_state == kQueryPendingOnGPU) {
        query_pending++;
        continue;
      }
      if (slot_state == kReadyForResetIssue) {
        ready_for_reset++;
        continue;
      }
      if (slot_state == kResetPendingOnGPU) {
        reset_pending++;
        continue;
      }
    }
    LOG("QUERY POOL STATE:\nready for query: %du\nquery pending: %du\nready for reset: %du\nreset "
        "pending: %du",
        ready_for_query, query_pending, ready_for_reset, reset_pending);
  }

 private:
  enum SlotState {
    kReadyForQueryIssue = 0,
    kQueryPendingOnGPU,
    kReadyForResetIssue,
    kResetPendingOnGPU,
  };

  static constexpr uint32_t kNumLogicalQuerySlots = 16384;
  static constexpr uint32_t kNumPhysicalTimerQuerySlots = kNumLogicalQuerySlots * 2;

  void ResetTimerQueryPool(const VkDevice& device, const VkPhysicalDevice& physical_device,
                           const VkQueryPool& query_pool);

  DispatchTable* dispatch_table_;
  QueueFamilyInfoManager* queue_family_info_manager_;
  PhysicalDeviceManager* physical_device_manager_;
  absl::Mutex mutex_;
  absl::flat_hash_map<VkDevice, VkQueryPool> device_to_query_pool_;

  absl::flat_hash_map<VkDevice, std::array<SlotState, kNumLogicalQuerySlots>>
      device_to_query_slots_;
  absl::flat_hash_map<VkDevice, uint32_t> device_to_potential_next_free_index_;

  absl::flat_hash_map<VkDevice, std::vector<uint32_t>> device_to_pending_reset_slots_;
  std::atomic<bool> reset_needed_ = false;
};
}  // namespace orbit_vulkan_layer

#endif  // ORBIT_VULKAN_LAYER_TIMER_QUERY_POOL_H_
