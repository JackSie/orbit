// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "QueueFamilyInfoManager.h"
void orbit_vulkan_layer::QueueFamilyInfoManager::InitializeQueueFamilyInfo(
    const VkPhysicalDevice& device) {
  LOG("InitializeQueueFamilyInfo");

  // We need to know the count of properties first, before the actual query.
  uint32_t properties_count = 0;
  dispatch_table_->GetPhysicalDeviceQueueFamilyProperties(device)(device, &properties_count,
                                                                  nullptr);
  CHECK(properties_count > 0);

  std::vector<VkQueueFamilyProperties> queue_family_properties;
  queue_family_properties.resize(properties_count);
  dispatch_table_->GetPhysicalDeviceQueueFamilyProperties(device)(device, &properties_count,
                                                                  queue_family_properties.data());

  const VkQueueFlags reset_query_pool_supported_flags =
      VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

  QueueFamilyInfo queue_family_info;
  for (uint32_t index = 0; index < queue_family_properties.size(); ++index) {
    VkQueueFamilyProperties properties = queue_family_properties[index];
    if (properties.timestampValidBits > 0) {
      queue_family_info.timestamp_supporting_queue_family_indices.insert(index);
    }
    if ((properties.queueFlags & reset_query_pool_supported_flags) != 0) {
      queue_family_info.reset_supporting_queue_family_indices.insert(index);
    }
  }
  queue_family_info.queue_family_properties = std::move(queue_family_properties);

  {
    absl::WriterMutexLock lock(&mutex_);
    CHECK(!device_to_queue_family_info_.contains(device));
    device_to_queue_family_info_[device] = std::move(queue_family_info);
  }
}

void orbit_vulkan_layer::QueueFamilyInfoManager::RemoveQueueFamilyInfo(
    const VkPhysicalDevice& device) {
  absl::WriterMutexLock lock(&mutex_);
  CHECK(device_to_queue_family_info_.contains(device));
  device_to_queue_family_info_.erase(device);
}

uint32_t orbit_vulkan_layer::QueueFamilyInfoManager::SuitableQueueFamilyIndexForQueryPoolReset(
    const VkPhysicalDevice& device) {
  LOG("SuitableQueueFamilyIndexForQueryPoolReset");
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_to_queue_family_info_.contains(device));
  const QueueFamilyInfo queue_family_info = device_to_queue_family_info_.at(device);
  CHECK(!queue_family_info.reset_supporting_queue_family_indices.empty());
  return *queue_family_info.reset_supporting_queue_family_indices.begin();
}
