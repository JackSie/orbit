// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "PhysicalDeviceManager.h"

#include "OrbitBase/Logging.h"

namespace orbit_vulkan_layer {
void PhysicalDeviceManager::TrackPhysicalDevices(const VkInstance& instance,
                                                 uint32_t* physical_device_count,
                                                 VkPhysicalDevice* physical_devices) {
  if (physical_device_count != nullptr && physical_devices != nullptr) {
    // Map these devices to this instance so that we can map each physical
    // device back to a dispatch table which is bound to the instance.
    // Note that this is hardly error-proof. Physical devices could be used by multiple instances
    // (in fact this is an n-to-n mapping).
    // In theory the dispatch table can also be different per instance, thus we could end-up in
    // calling the wrong function, but there is no perfect solution for this, as we do not have
    // any other chance to know which instance is the right one at the call.
    {
      absl::WriterMutexLock lock(&mutex_);
      for (uint32_t i = 0; i < *physical_device_count; ++i) {
        const VkPhysicalDevice& device = physical_devices[i];
        physical_device_to_instance_[device] = instance;
        VkPhysicalDeviceProperties properties;
        dispatch_table_->GetPhysicalDeviceProperties(instance)(device, &properties);
        physical_device_to_properties_[device] = properties;
      }
    }
  }
}

VkInstance orbit_vulkan_layer::PhysicalDeviceManager::GetInstanceOfPhysicalDevice(
    const VkPhysicalDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(physical_device_to_instance_.contains(device));
  return physical_device_to_instance_.at(device);
}

VkPhysicalDeviceProperties PhysicalDeviceManager::GetPhysicalDeviceProperties(
    const VkPhysicalDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(physical_device_to_properties_.contains(device));
  return physical_device_to_properties_.at(device);
}

}  // namespace orbit_vulkan_layer