// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "PhysicalDeviceManager.h"

#include "OrbitBase/Logging.h"

namespace orbit_vulkan_layer {

VkPhysicalDeviceProperties PhysicalDeviceManager::GetPhysicalDeviceProperties(
    const VkPhysicalDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(physical_device_to_properties_.contains(device));
  return physical_device_to_properties_.at(device);
}

}  // namespace orbit_vulkan_layer