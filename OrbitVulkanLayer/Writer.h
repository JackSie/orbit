// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ORBIT_VULKAN_LAYER_WRITER_H_
#define ORBIT_VULKAN_LAYER_WRITER_H_

#include <fstream>
#include <iosfwd>
#include <string>
#include <utility>

#include "OrbitBase/Logging.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "capture.grpc.pb.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/message.h"

namespace orbit_vulkan_layer {
class Writer {
 public:
  explicit Writer(std::string filename) : filename_(std::move(filename)) {}

  void WriteQueueSubmission(const orbit_grpc_protos::GpuQueueSubmisssion& submisssion) {
    LOG("WriteQueueSubmission");
    orbit_grpc_protos::CaptureEvent event;
    event.mutable_gpu_queue_submission()->CopyFrom(submisssion);
    WriteMessage(event);
  }

  uint64_t InternStringIfNecessaryAndGetKey(std::string str) {
    uint64_t key = ComputeStringKey(str);
    {
      absl::MutexLock lock{&string_keys_sent_mutex_};
      if (string_keys_sent_.contains(key)) {
        return key;
      }
      string_keys_sent_.emplace(key);
    }

    orbit_grpc_protos::CaptureEvent event;
    event.mutable_interned_string()->set_key(key);
    event.mutable_interned_string()->set_intern(std::move(str));
    WriteMessage(event);
    return key;
  }

  void ClearStringInternPool() {
    absl::MutexLock lock{&string_keys_sent_mutex_};
    string_keys_sent_.clear();
  }

 private:
  void WriteMessage(const google::protobuf::Message& message) {
    LOG("WriteMessage");
    std::ofstream stream(filename_, std::ios::app | std::ios::binary);
    google::protobuf::io::OstreamOutputStream out_stream(&stream);
    google::protobuf::io::CodedOutputStream coded_output(&out_stream);
    uint32_t message_size = message.ByteSizeLong();
    coded_output.WriteLittleEndian32(message_size);
    message.SerializeToCodedStream(&coded_output);
  }

  static uint64_t ComputeStringKey(const std::string& str) { return std::hash<std::string>{}(str); }

  std::string filename_;
  absl::flat_hash_set<uint64_t> string_keys_sent_;
  absl::Mutex string_keys_sent_mutex_;
};

}  // namespace orbit_vulkan_layer

#endif  // ORBIT_VULKAN_LAYER_WRITER_H_
