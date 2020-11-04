// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_VULKAN_LAYER_ORBIT_CONNECTOR_H_
#define ORBIT_VULKAN_LAYER_ORBIT_CONNECTOR_H_

#include <OrbitBase/Logging.h>
#include <unistd.h>

#include <atomic>
#include <fstream>
#include <thread>

namespace orbit_vulkan_layer {

class OrbitConnector {
 public:
  OrbitConnector() : checker_thread_(CheckIsCapturing, &exit_requested_, &is_capturing_) {}
  [[nodiscard]] bool IsCapturing() const { return is_capturing_; }

 private:
  static void CheckIsCapturing(std::atomic<bool>* exit_requested, std::atomic<bool>* is_capturing) {
    LOG("CheckIsCapturing");
    while (!*exit_requested) {
      {
        std::ifstream f("/mnt/developer/orbit_layer_lock");
        *is_capturing = f.good();
      }
      usleep(10000);
    }
  }
  std::atomic<bool> is_capturing_ = false;
  std::atomic<bool> exit_requested_ = false;
  std::thread checker_thread_;
};

}  // namespace orbit_vulkan_layer

#endif  // ORBIT_VULKAN_LAYER_ORBIT_CONNECTOR_H_
