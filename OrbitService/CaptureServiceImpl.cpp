// Copyright (c) 2020 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CaptureServiceImpl.h"

#include <cstdio>
#include <iostream>

#include "CaptureEventBuffer.h"
#include "CaptureEventSender.h"
#include "LinuxTracingHandler.h"
#include "OrbitBase/Logging.h"
#include "OrbitBase/MakeUniqueForOverwrite.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/message.h"

namespace orbit_service {

using orbit_grpc_protos::CaptureRequest;
using orbit_grpc_protos::CaptureResponse;

namespace {

using orbit_grpc_protos::CaptureEvent;

class SenderThreadCaptureEventBuffer final : public CaptureEventBuffer {
 public:
  explicit SenderThreadCaptureEventBuffer(CaptureEventSender* event_sender)
      : capture_event_sender_{event_sender} {
    CHECK(capture_event_sender_ != nullptr);
    sender_thread_ = std::thread{[this] { SenderThread(); }};
  }

  void AddEvent(orbit_grpc_protos::CaptureEvent&& event) override {
    absl::MutexLock lock{&event_buffer_mutex_};
    if (stop_requested_) {
      return;
    }
    event_buffer_.emplace_back(std::move(event));
  }

  void StopAndWait() {
    CHECK(sender_thread_.joinable());
    {
      // Protect stop_requested_ with event_buffer_mutex_ so that we can use stop_requested_
      // in Conditions for Await/LockWhen (specifically, in SenderThread).
      absl::MutexLock lock{&event_buffer_mutex_};
      stop_requested_ = true;
    }
    sender_thread_.join();
  }

  ~SenderThreadCaptureEventBuffer() override { CHECK(!sender_thread_.joinable()); }

 private:
  bool ReadMessage(google::protobuf::Message* message,
                   google::protobuf::io::CodedInputStream* input) {
    uint32_t message_size;
    if (!input->ReadLittleEndian32(&message_size)) {
      return false;
    }

    std::unique_ptr<char[]> buffer = make_unique_for_overwrite<char[]>(message_size);
    if (!input->ReadRaw(buffer.get(), message_size)) {
      return false;
    }
    message->ParseFromArray(buffer.get(), message_size);

    return true;
  }

  void SenderThread() {
    pthread_setname_np(pthread_self(), "SenderThread");
    constexpr absl::Duration kSendTimeInterval = absl::Milliseconds(20);
    // This should be lower than kMaxEventsPerResponse in GrpcCaptureEventSender::SendEvents
    // as a few more events are likely to arrive after the condition becomes true.
    constexpr uint64_t kSendEventCountInterval = 5000;

    bool stopped = false;
    while (!stopped) {
      ORBIT_SCOPE("SenderThread iteration");
      event_buffer_mutex_.LockWhenWithTimeout(absl::Condition(
                                                  +[](SenderThreadCaptureEventBuffer* self) {
                                                    return self->event_buffer_.size() >=
                                                               kSendEventCountInterval ||
                                                           self->stop_requested_;
                                                  },
                                                  this),
                                              kSendTimeInterval);
      if (stop_requested_) {
        stopped = true;

        // now read the vulkan layer result:
        const std::string file_name = "/mnt/developer/orbit_test_file";
        std::ifstream file(file_name, std::ios::binary);
        if (file.good()) {
          google::protobuf::io::IstreamInputStream input_stream(&file);
          google::protobuf::io::CodedInputStream coded_input(&input_stream);

          orbit_grpc_protos::GpuQueueSubmisssion queue_submission;
          while (ReadMessage(&queue_submission, &coded_input)) {
            CaptureEvent event;
            event.mutable_gpu_queue_submission()->CopyFrom(queue_submission);
            event_buffer_.emplace_back(std::move(event));
          }

          file.close();
          std::remove(file_name.c_str());
        }
      }
      std::vector<CaptureEvent> buffered_events = std::move(event_buffer_);
      event_buffer_.clear();
      event_buffer_mutex_.Unlock();
      capture_event_sender_->SendEvents(std::move(buffered_events));
    }
  }

  std::vector<orbit_grpc_protos::CaptureEvent> event_buffer_;
  absl::Mutex event_buffer_mutex_;
  CaptureEventSender* capture_event_sender_;
  std::thread sender_thread_;
  bool stop_requested_ = false;
};

class GrpcCaptureEventSender final : public CaptureEventSender {
 public:
  explicit GrpcCaptureEventSender(
      grpc::ServerReaderWriter<CaptureResponse, CaptureRequest>* reader_writer)
      : reader_writer_{reader_writer} {
    CHECK(reader_writer_ != nullptr);
  }

  void SendEvents(std::vector<orbit_grpc_protos::CaptureEvent>&& events) override {
    ORBIT_SCOPE_FUNCTION;
    ORBIT_UINT64("Number of sent buffered events", events.size());
    if (events.empty()) {
      return;
    }

    constexpr uint64_t kMaxEventsPerResponse = 10'000;
    CaptureResponse response;
    for (CaptureEvent& event : events) {
      // We buffer to avoid sending countless tiny messages, but we also want to
      // avoid huge messages, which would cause the capture on the client to jump
      // forward in time in few big steps and not look live anymore.
      if (response.capture_events_size() == kMaxEventsPerResponse) {
        reader_writer_->Write(response);
        response.clear_capture_events();
      }
      response.mutable_capture_events()->Add(std::move(event));
    }
    reader_writer_->Write(response);
  }

 private:
  grpc::ServerReaderWriter<CaptureResponse, CaptureRequest>* reader_writer_;
};

}  // namespace

grpc::Status CaptureServiceImpl::Capture(
    grpc::ServerContext*,
    grpc::ServerReaderWriter<CaptureResponse, CaptureRequest>* reader_writer) {
  pthread_setname_np(pthread_self(), "CSImpl::Capture");
  if (is_capturing) {
    ERROR("Cannot start capture because another capture is already in progress");
    return grpc::Status(grpc::StatusCode::ALREADY_EXISTS,
                        "Cannot start capture because another capture is already in progress.");
  }
  is_capturing = true;

  GrpcCaptureEventSender capture_event_sender{reader_writer};
  SenderThreadCaptureEventBuffer capture_event_buffer{&capture_event_sender};
  LinuxTracingHandler tracing_handler{&capture_event_buffer};

  CaptureRequest request;
  reader_writer->Read(&request);
  LOG("Read CaptureRequest from Capture's gRPC stream: starting capture");
  tracing_handler.Start(std::move(*request.mutable_capture_options()));

  {
    LOG("Requesting Vulkan Layer To Writer!?");
    std::ofstream layer_start_capture_file("/mnt/developer/orbit_layer_lock");
    layer_start_capture_file << "lock" << std::endl;
    layer_start_capture_file.close();
  }

  // The client asks for the capture to be stopped by calling WritesDone.
  // At that point, this call to Read will return false.
  // In the meantime, it blocks if no message is received.
  while (reader_writer->Read(&request)) {
  }
  LOG("Client finished writing on Capture's gRPC stream: stopping capture");
  std::remove("/mnt/developer/orbit_layer_lock");
  tracing_handler.Stop();
  LOG("LinuxTracingHandler stopped: perf_event_open tracing is done");

  capture_event_buffer.StopAndWait();
  LOG("Finished handling gRPC call to Capture: all capture data has been sent");
  is_capturing = false;
  return grpc::Status::OK;
}

}  // namespace orbit_service
