// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_VULKAN_LAYER_WRITER_H_
#define ORBIT_VULKAN_LAYER_WRITER_H_

#include <fstream>
#include <iosfwd>
#include <string>

#include "OrbitBase/Logging.h"
#include "absl/synchronization/mutex.h"
#include "capture.grpc.pb.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/message.h"

namespace orbit_vulkan_layer {
class Writer {
 public:
  explicit Writer(const std::string& filename) : filename_(std::move(filename)) {}

  void WriteCommandBuffer(uint64_t gpu_begin_ns, uint64_t gpu_end_ns, int64_t gpu_cpu_diff_approx) {
    LOG("WriteCommandBuffer");
    absl::MutexLock lock(&mutex_);
    orbit_grpc_protos::GpuCommandBuffer command_buffer_event;
    command_buffer_event.set_begin_gpu_timestamp_ns(gpu_begin_ns);
    command_buffer_event.set_approx_begin_cpu_timestamp_ns(gpu_begin_ns + gpu_cpu_diff_approx);
    command_buffer_event.set_end_gpu_timestamp_ns(gpu_end_ns);
    command_buffer_event.set_approx_end_cpu_timestamp_ns(gpu_end_ns + gpu_cpu_diff_approx);

    WriteMessage(&command_buffer_event);
  }

 private:
  void WriteMessage(const google::protobuf::Message* message) {
    LOG("WriteMessage");
    std::ofstream stream(filename_, std::ios::app | std::ios::binary);
    google::protobuf::io::OstreamOutputStream out_stream(&stream);
    google::protobuf::io::CodedOutputStream coded_output(&out_stream);
    uint32_t message_size = message->ByteSizeLong();
    coded_output.WriteLittleEndian32(message_size);
    message->SerializeToCodedStream(&coded_output);
  }

  absl::Mutex mutex_;

  std::string filename_;
};

}  // namespace orbit_vulkan_layer

#endif  // ORBIT_VULKAN_LAYER_WRITER_H_
