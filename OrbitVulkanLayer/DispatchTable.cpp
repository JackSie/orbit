// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "DispatchTable.h"

#include "OrbitBase/Logging.h"

namespace orbit_vulkan_layer {

void DispatchTable::CreateInstanceDispatchTable(
    const VkInstance& instance,
    const PFN_vkGetInstanceProcAddr& next_get_instance_proc_addr_function) {
  VkLayerInstanceDispatchTable dispatch_table;
  dispatch_table.DestroyInstance = absl::bit_cast<PFN_vkDestroyInstance>(
      next_get_instance_proc_addr_function(instance, "vkDestroyInstance"));
  dispatch_table.GetInstanceProcAddr = absl::bit_cast<PFN_vkGetInstanceProcAddr>(
      next_get_instance_proc_addr_function(instance, "vkGetInstanceProcAddr"));
  dispatch_table.EnumerateDeviceExtensionProperties =
      absl::bit_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          next_get_instance_proc_addr_function(instance, "vkEnumerateDeviceExtensionProperties"));
  dispatch_table.EnumeratePhysicalDevices = absl::bit_cast<PFN_vkEnumeratePhysicalDevices>(
      next_get_instance_proc_addr_function(instance, "vkEnumeratePhysicalDevices"));
  dispatch_table.GetPhysicalDeviceQueueFamilyProperties =
      absl::bit_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          next_get_instance_proc_addr_function(instance,
                                               "vkGetPhysicalDeviceQueueFamilyProperties"));
  dispatch_table.GetPhysicalDeviceProperties = absl::bit_cast<PFN_vkGetPhysicalDeviceProperties>(
      next_get_instance_proc_addr_function(instance, "vkGetPhysicalDeviceProperties"));

  {
    absl::WriterMutexLock lock(&mutex_);
    instance_dispatch_table_[instance] = dispatch_table;
  }
}

void DispatchTable::RemoveInstanceDispatchTable(const VkInstance& instance) {
  absl::WriterMutexLock lock(&mutex_);
  instance_dispatch_table_.erase(instance);
}

void DispatchTable::CreateDeviceDispatchTable(
    const VkDevice& device, const PFN_vkGetDeviceProcAddr& next_get_device_proc_add_function) {
  VkLayerDispatchTable dispatch_table;

  dispatch_table.GetDeviceProcAddr = absl::bit_cast<PFN_vkGetDeviceProcAddr>(
      next_get_device_proc_add_function(device, "vkGetDeviceProcAddr"));

  dispatch_table.CreateCommandPool = absl::bit_cast<PFN_vkCreateCommandPool>(
      next_get_device_proc_add_function(device, "vkCreateCommandPool"));
  dispatch_table.DestroyCommandPool = absl::bit_cast<PFN_vkDestroyCommandPool>(
      next_get_device_proc_add_function(device, "vkDestroyCommandPool"));
  dispatch_table.ResetCommandPool = absl::bit_cast<PFN_vkResetCommandPool>(
      next_get_device_proc_add_function(device, "vkResetCommandPool"));

  dispatch_table.AllocateCommandBuffers = absl::bit_cast<PFN_vkAllocateCommandBuffers>(
      next_get_device_proc_add_function(device, "vkAllocateCommandBuffers"));
  dispatch_table.FreeCommandBuffers = absl::bit_cast<PFN_vkFreeCommandBuffers>(
      next_get_device_proc_add_function(device, "vkFreeCommandBuffers"));
  dispatch_table.BeginCommandBuffer = absl::bit_cast<PFN_vkBeginCommandBuffer>(
      next_get_device_proc_add_function(device, "vkBeginCommandBuffer"));
  dispatch_table.EndCommandBuffer = absl::bit_cast<PFN_vkEndCommandBuffer>(
      next_get_device_proc_add_function(device, "vkEndCommandBuffer"));
  dispatch_table.ResetCommandBuffer = absl::bit_cast<PFN_vkResetCommandBuffer>(
      next_get_device_proc_add_function(device, "vkResetCommandBuffer"));

  dispatch_table.QueueSubmit =
      absl::bit_cast<PFN_vkQueueSubmit>(next_get_device_proc_add_function(device, "vkQueueSubmit"));
  dispatch_table.QueuePresentKHR = absl::bit_cast<PFN_vkQueuePresentKHR>(
      next_get_device_proc_add_function(device, "vkQueuePresentKHR"));

  dispatch_table.GetDeviceQueue = absl::bit_cast<PFN_vkGetDeviceQueue>(
      next_get_device_proc_add_function(device, "vkGetDeviceQueue"));
  dispatch_table.GetDeviceQueue2 = absl::bit_cast<PFN_vkGetDeviceQueue2>(
      next_get_device_proc_add_function(device, "vkGetDeviceQueue2"));

  dispatch_table.CreateQueryPool = absl::bit_cast<PFN_vkCreateQueryPool>(
      next_get_device_proc_add_function(device, "vkCreateQueryPool"));
  dispatch_table.CmdResetQueryPool = absl::bit_cast<PFN_vkCmdResetQueryPool>(
      next_get_device_proc_add_function(device, "vkCmdResetQueryPool"));

  dispatch_table.CmdWriteTimestamp = absl::bit_cast<PFN_vkCmdWriteTimestamp>(
      next_get_device_proc_add_function(device, "vkCmdWriteTimestamp"));

  dispatch_table.CmdBeginQuery = absl::bit_cast<PFN_vkCmdBeginQuery>(
      next_get_device_proc_add_function(device, "vkCmdBeginQuery"));
  dispatch_table.CmdEndQuery =
      absl::bit_cast<PFN_vkCmdEndQuery>(next_get_device_proc_add_function(device, "vkCmdEndQuery"));
  dispatch_table.GetQueryPoolResults = absl::bit_cast<PFN_vkGetQueryPoolResults>(
      next_get_device_proc_add_function(device, "vkGetQueryPoolResults"));

  dispatch_table.CreateFence =
      absl::bit_cast<PFN_vkCreateFence>(next_get_device_proc_add_function(device, "vkCreateFence"));
  dispatch_table.DestroyFence = absl::bit_cast<PFN_vkDestroyFence>(
      next_get_device_proc_add_function(device, "vkDestroyFence"));
  dispatch_table.GetFenceStatus = absl::bit_cast<PFN_vkGetFenceStatus>(
      next_get_device_proc_add_function(device, "vkGetFenceStatus"));

  dispatch_table.CreateEvent =
      absl::bit_cast<PFN_vkCreateEvent>(next_get_device_proc_add_function(device, "vkCreateEvent"));
  dispatch_table.DestroyEvent = absl::bit_cast<PFN_vkDestroyEvent>(
      next_get_device_proc_add_function(device, "vkDestroyEvent"));
  dispatch_table.SetEvent =
      absl::bit_cast<PFN_vkSetEvent>(next_get_device_proc_add_function(device, "vkSetEvent"));
  dispatch_table.GetEventStatus = absl::bit_cast<PFN_vkGetEventStatus>(
      next_get_device_proc_add_function(device, "vkGetEventStatus"));
  dispatch_table.CmdWaitEvents = absl::bit_cast<PFN_vkCmdWaitEvents>(
      next_get_device_proc_add_function(device, "vkCmdWaitEvents"));
  dispatch_table.CmdSetEvent =
      absl::bit_cast<PFN_vkCmdSetEvent>(next_get_device_proc_add_function(device, "vkCmdSetEvent"));

  dispatch_table.CmdPipelineBarrier = absl::bit_cast<PFN_vkCmdPipelineBarrier>(
      next_get_device_proc_add_function(device, "vkCmdPipelineBarrier"));

  {
    absl::WriterMutexLock lock(&mutex_);
    device_dispatch_table_[device] = dispatch_table;
  }
}

void DispatchTable::RemoveDeviceDispatchTable(const VkDevice& device) {
  absl::WriterMutexLock lock(&mutex_);
  device_dispatch_table_.erase(device);
}

PFN_vkDestroyDevice DispatchTable::DestroyDevice(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).DestroyDevice;
}

PFN_vkDestroyInstance DispatchTable::DestroyInstance(const VkInstance& instance) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(instance_dispatch_table_.contains(instance));
  return instance_dispatch_table_.at(instance).DestroyInstance;
}

PFN_vkEnumerateDeviceExtensionProperties DispatchTable::EnumerateDeviceExtensionProperties(
    const VkInstance& instance) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(instance_dispatch_table_.contains(instance));
  return instance_dispatch_table_.at(instance).EnumerateDeviceExtensionProperties;
}

PFN_vkEnumeratePhysicalDevices DispatchTable::EnumeratePhysicalDevices(const VkInstance& instance) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(instance_dispatch_table_.contains(instance));
  return instance_dispatch_table_.at(instance).EnumeratePhysicalDevices;
}

PFN_vkGetPhysicalDeviceQueueFamilyProperties DispatchTable::GetPhysicalDeviceQueueFamilyProperties(
    VkInstance const& instance) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(instance_dispatch_table_.contains(instance));
  return instance_dispatch_table_.at(instance).GetPhysicalDeviceQueueFamilyProperties;
}

PFN_vkGetPhysicalDeviceProperties DispatchTable::GetPhysicalDeviceProperties(
    const VkInstance& instance) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(instance_dispatch_table_.contains(instance));
  return instance_dispatch_table_.at(instance).GetPhysicalDeviceProperties;
}

PFN_vkGetInstanceProcAddr DispatchTable::GetInstanceProcAddr(const VkInstance& instance) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(instance_dispatch_table_.contains(instance));
  return instance_dispatch_table_.at(instance).GetInstanceProcAddr;
}

PFN_vkGetDeviceProcAddr DispatchTable::GetDeviceProcAddr(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).GetDeviceProcAddr;
}

PFN_vkCreateCommandPool DispatchTable::CreateCommandPool(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CreateCommandPool;
}

PFN_vkDestroyCommandPool DispatchTable::DestroyCommandPool(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).DestroyCommandPool;
}

PFN_vkResetCommandPool DispatchTable::ResetCommandPool(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).ResetCommandPool;
}

PFN_vkAllocateCommandBuffers DispatchTable::AllocateCommandBuffers(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).AllocateCommandBuffers;
}

PFN_vkFreeCommandBuffers DispatchTable::FreeCommandBuffers(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).FreeCommandBuffers;
}

PFN_vkBeginCommandBuffer DispatchTable::BeginCommandBuffer(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).BeginCommandBuffer;
}

PFN_vkEndCommandBuffer DispatchTable::EndCommandBuffer(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).EndCommandBuffer;
}

PFN_vkResetCommandBuffer DispatchTable::ResetCommandBuffer(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).ResetCommandBuffer;
}

PFN_vkGetDeviceQueue DispatchTable::GetDeviceQueue(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).GetDeviceQueue;
}

PFN_vkGetDeviceQueue2 DispatchTable::GetDeviceQueue2(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).GetDeviceQueue2;
}

PFN_vkQueueSubmit DispatchTable::QueueSubmit(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).QueueSubmit;
}

PFN_vkQueuePresentKHR DispatchTable::QueuePresentKHR(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).QueuePresentKHR;
}

PFN_vkCreateQueryPool DispatchTable::CreateQueryPool(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CreateQueryPool;
}

PFN_vkCmdResetQueryPool DispatchTable::CmdResetQueryPool(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CmdResetQueryPool;
}

PFN_vkGetQueryPoolResults DispatchTable::GetQueryPoolResults(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).GetQueryPoolResults;
}

PFN_vkCreateFence DispatchTable::CreateFence(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CreateFence;
}

PFN_vkDestroyFence DispatchTable::DestroyFence(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).DestroyFence;
}

PFN_vkGetFenceStatus DispatchTable::GetFenceStatus(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).GetFenceStatus;
}

PFN_vkCmdWriteTimestamp DispatchTable::CmdWriteTimestamp(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CmdWriteTimestamp;
}

PFN_vkCreateEvent DispatchTable::CreateEvent(VkDevice const& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CreateEvent;
}

PFN_vkDestroyEvent DispatchTable::DestroyEvent(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).DestroyEvent;
}

PFN_vkSetEvent DispatchTable::SetEvent(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).SetEvent;
}

PFN_vkGetEventStatus DispatchTable::GetEventStatus(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).GetEventStatus;
}

PFN_vkCmdWaitEvents DispatchTable::CmdWaitEvents(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CmdWaitEvents;
}

PFN_vkCmdSetEvent DispatchTable::CmdSetEvent(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CmdSetEvent;
}
PFN_vkCmdPipelineBarrier DispatchTable::CmdPipelineBarrier(const VkDevice& device) {
  absl::ReaderMutexLock lock(&mutex_);
  CHECK(device_dispatch_table_.contains(device));
  return device_dispatch_table_.at(device).CmdPipelineBarrier;
}

}  // namespace orbit_vulkan_layer