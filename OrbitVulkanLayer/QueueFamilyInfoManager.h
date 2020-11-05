// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_VULKAN_LAYER_QUEUE_FAMILY_INFO_MANAGER_H_
#define ORBIT_VULKAN_LAYER_QUEUE_FAMILY_INFO_MANAGER_H_

#include "DispatchTable.h"
#include "OrbitBase/Logging.h"
#include "PhysicalDeviceManager.h"
#include "absl/container/node_hash_map.h"
#include "absl/synchronization/mutex.h"

namespace orbit_vulkan_layer {

struct QueueFamilyInfo {
  std::vector<VkQueueFamilyProperties> queue_family_properties;
  absl::flat_hash_set<uint32_t> timestamp_supporting_queue_family_indices;
  absl::flat_hash_set<uint32_t> reset_supporting_queue_family_indices;
};

class QueueFamilyInfoManager {
 public:
  explicit QueueFamilyInfoManager(DispatchTable* dispatch_table)
      : dispatch_table_(dispatch_table) {}

  void InitializeQueueFamilyInfo(const VkPhysicalDevice& device);

  uint32_t SuitableQueueFamilyIndexForQueryPoolReset(const VkPhysicalDevice& device);

  void RemoveQueueFamilyInfo(const VkPhysicalDevice& device);

 private:
  DispatchTable* dispatch_table_;
  absl::Mutex mutex_;
  absl::node_hash_map<VkPhysicalDevice, QueueFamilyInfo> device_to_queue_family_info_;
};

}  // namespace orbit_vulkan_layer

#endif  // ORBIT_VULKAN_LAYER_QUEUE_FAMILY_INFO_MANAGER_H_
