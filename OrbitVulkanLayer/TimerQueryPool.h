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

  void ResetSlots(const VkDevice& device, const std::vector<uint32_t>& physical_slot_indices) {
    if (physical_slot_indices.empty()) {
      return;
    }
    LOG("ResetSlots");
    absl::WriterMutexLock lock(&mutex_);
    std::array<SlotState, kNumLogicalQuerySlots>& slot_states = device_to_query_slots_.at(device);
    for (uint32_t physical_slot_index : physical_slot_indices) {
      CHECK(physical_slot_index < kNumPhysicalTimerQuerySlots);
      bool is_base_slot = (physical_slot_index % 2) == 0;
      CHECK(is_base_slot);
      uint32_t logical_slot_index = physical_slot_index / 2;
      const SlotState& current_state = slot_states.at(logical_slot_index);
      CHECK(current_state == kQueryPendingOnGPU);
      const VkQueryPool& query_pool = device_to_query_pool_.at(device);
      dispatch_table_->ResetQueryPoolEXT(device)(device, query_pool, physical_slot_index, 2);
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

 private:
  enum SlotState {
    kReadyForQueryIssue = 0,
    kQueryPendingOnGPU,
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
