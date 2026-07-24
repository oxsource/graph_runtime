#include "src/framework/scheduler/input_stream_handler.h"

#include <algorithm>

namespace graph::runtime {

// --- SyncSet ---

SyncSet::SyncSet(std::vector<InputStreamManager*> managers)
    : managers_(std::move(managers)) {}

SyncSet::Readiness SyncSet::GetReadiness(Timestamp* min_stream_timestamp) {
  Timestamp min_bound = Timestamp::Done();
  Timestamp min_packet = Timestamp::Done();
  bool any_bound_empty = false;

  for (auto* mgr : managers_) {
    bool empty;
    Timestamp ts = mgr->MinTimestampOrBound(&empty);
    if (empty) {
      min_bound = std::min(min_bound, ts);
      any_bound_empty = true;
    } else {
      min_packet = std::min(min_packet, ts);
    }
  }

  *min_stream_timestamp = std::min(min_packet, min_bound);

  if (*min_stream_timestamp >= Timestamp::OneOverPostStream()) {
    return kReadyForClose;
  }

  if (!any_bound_empty) {
    return kReadyForProcess;
  }

  if (min_bound > min_packet) {
    return kReadyForProcess;
  }

  return kNotReady;
}

void SyncSet::FillInputSet(Timestamp timestamp, GraphContext& context) {
  for (size_t i = 0; i < managers_.size(); ++i) {
    auto* mgr = managers_[i];
    int num_dropped;
    bool stream_is_done;
    mgr->PopPacketAtTimestamp(timestamp, &num_dropped, &stream_is_done);
    // FillInputSet is called to pop packets from managers.
    // The actual shard population happens in the Scheduler's task runner.
  }
}

void SyncSet::FillInputBounds(GraphContext& context) {
  for (size_t i = 0; i < managers_.size(); ++i) {
    bool empty;
    managers_[i]->MinTimestampOrBound(&empty);
  }
}

// --- DefaultInputStreamHandler ---

DefaultInputStreamHandler::DefaultInputStreamHandler() {}

void DefaultInputStreamHandler::SetInputStreamManagers(
    const std::vector<InputStreamManager*>& managers) {
  managers_ = managers;
  sync_set_ = std::make_unique<SyncSet>(managers);
}

void DefaultInputStreamHandler::SetScheduleCallback(ScheduleCallback cb) {
  schedule_callback_ = std::move(cb);
}

bool DefaultInputStreamHandler::ScheduleInvocations(
    int max_allowance, Timestamp* input_bound,
    Node& node, GraphContext& context) {
  int scheduled = 0;
  while (scheduled < max_allowance) {
    Timestamp min_ts;
    Readiness r = GetNodeReadiness(&min_ts);
    if (r == kReadyForProcess) {
      if (schedule_callback_) {
        schedule_callback_(node);
      }
      ++scheduled;
    } else if (r == kReadyForClose) {
      if (schedule_callback_) {
        schedule_callback_(node);
      }
      break;
    } else {
      *input_bound = min_ts;
      break;
    }
  }
  return scheduled > 0;
}

InputStreamHandler::Readiness DefaultInputStreamHandler::GetNodeReadiness(
    Timestamp* min_stream_timestamp) {
  if (!sync_set_) return kNotReady;
  return static_cast<Readiness>(
      sync_set_->GetReadiness(min_stream_timestamp));
}

void DefaultInputStreamHandler::FillInputSet(
    Timestamp timestamp, GraphContext& context) {
  if (sync_set_) {
    sync_set_->FillInputSet(timestamp, context);
  }
}

void DefaultInputStreamHandler::NotifyPacketArrival() {
  notified_ = true;
}

void DefaultInputStreamHandler::SetNextTimestampBound(
    CollectionItemId id, Timestamp bound) {
  if (id >= 0 && id < static_cast<int>(managers_.size())) {
    managers_[id]->SetNextTimestampBound(bound);
  }
}

absl::Status DefaultInputStreamHandler::AddPacketsToStream(
    CollectionItemId id, const std::list<Packet>& packets, bool* notify) {
  if (id < 0 || id >= static_cast<int>(managers_.size())) {
    return absl::InvalidArgumentError("Invalid stream id");
  }
  return managers_[id]->AddPackets(packets, notify);
}

absl::Status DefaultInputStreamHandler::MovePacketsToStream(
    CollectionItemId id, std::list<Packet>* packets, bool* notify) {
  if (id < 0 || id >= static_cast<int>(managers_.size())) {
    return absl::InvalidArgumentError("Invalid stream id");
  }
  return managers_[id]->MovePackets(packets, notify);
}

void DefaultInputStreamHandler::Close() {
  for (auto* mgr : managers_) {
    mgr->Close();
  }
}

// --- ImmediateInputStreamHandler ---

ImmediateInputStreamHandler::ImmediateInputStreamHandler() {}

void ImmediateInputStreamHandler::SetInputStreamManagers(
    const std::vector<InputStreamManager*>& managers) {
  managers_ = managers;
}

void ImmediateInputStreamHandler::SetScheduleCallback(ScheduleCallback cb) {
  schedule_callback_ = std::move(cb);
}

bool ImmediateInputStreamHandler::ScheduleInvocations(
    int max_allowance, Timestamp* input_bound,
    Node& node, GraphContext& context) {
  int scheduled = 0;
  while (scheduled < max_allowance) {
    Timestamp min_ts;
    Readiness r = GetNodeReadiness(&min_ts);
    if (r == kReadyForProcess) {
      if (schedule_callback_) schedule_callback_(node);
      ++scheduled;
    } else if (r == kReadyForClose) {
      if (schedule_callback_) schedule_callback_(node);
      break;
    } else {
      *input_bound = min_ts;
      break;
    }
  }
  return scheduled > 0;
}

InputStreamHandler::Readiness ImmediateInputStreamHandler::GetNodeReadiness(
    Timestamp* min_stream_timestamp) {
  *min_stream_timestamp = Timestamp::Done();
  for (auto* mgr : managers_) {
    bool empty;
    Timestamp ts = mgr->MinTimestampOrBound(&empty);
    if (!empty) {
      *min_stream_timestamp = std::min(*min_stream_timestamp, ts);
      return kReadyForProcess;
    }
    *min_stream_timestamp = std::min(*min_stream_timestamp, ts);
  }
  return kNotReady;
}

void ImmediateInputStreamHandler::FillInputSet(
    Timestamp timestamp, GraphContext& context) {
  for (auto* mgr : managers_) {
    int num_dropped;
    bool stream_is_done;
    mgr->PopPacketAtTimestamp(timestamp, &num_dropped, &stream_is_done);
  }
}

void ImmediateInputStreamHandler::NotifyPacketArrival() {
  notified_ = true;
}

void ImmediateInputStreamHandler::SetNextTimestampBound(
    CollectionItemId id, Timestamp bound) {
  if (id >= 0 && id < static_cast<int>(managers_.size())) {
    managers_[id]->SetNextTimestampBound(bound);
  }
}

absl::Status ImmediateInputStreamHandler::AddPacketsToStream(
    CollectionItemId id, const std::list<Packet>& packets, bool* notify) {
  if (id < 0 || id >= static_cast<int>(managers_.size())) {
    return absl::InvalidArgumentError("Invalid stream id");
  }
  return managers_[id]->AddPackets(packets, notify);
}

absl::Status ImmediateInputStreamHandler::MovePacketsToStream(
    CollectionItemId id, std::list<Packet>* packets, bool* notify) {
  if (id < 0 || id >= static_cast<int>(managers_.size())) {
    return absl::InvalidArgumentError("Invalid stream id");
  }
  return managers_[id]->MovePackets(packets, notify);
}

void ImmediateInputStreamHandler::Close() {
  for (auto* mgr : managers_) mgr->Close();
}

// --- FixedSizeInputStreamHandler ---

FixedSizeInputStreamHandler::FixedSizeInputStreamHandler(int max_queue_size)
    : max_queue_size_(max_queue_size) {}

void FixedSizeInputStreamHandler::SetInputStreamManagers(
    const std::vector<InputStreamManager*>& managers) {
  managers_ = managers;
  for (auto* mgr : managers_) {
    if (max_queue_size_ > 0) mgr->SetMaxQueueSize(max_queue_size_);
  }
}

void FixedSizeInputStreamHandler::SetScheduleCallback(ScheduleCallback cb) {
  schedule_callback_ = std::move(cb);
}

bool FixedSizeInputStreamHandler::ScheduleInvocations(
    int max_allowance, Timestamp* input_bound,
    Node& node, GraphContext& context) {
  int scheduled = 0;
  while (scheduled < max_allowance) {
    Timestamp min_ts;
    Readiness r = GetNodeReadiness(&min_ts);
    if (r == kReadyForProcess) {
      if (schedule_callback_) schedule_callback_(node);
      ++scheduled;
    } else if (r == kReadyForClose) {
      if (schedule_callback_) schedule_callback_(node);
      break;
    } else {
      *input_bound = min_ts;
      break;
    }
  }
  return scheduled > 0;
}

InputStreamHandler::Readiness FixedSizeInputStreamHandler::GetNodeReadiness(
    Timestamp* min_stream_timestamp) {
  *min_stream_timestamp = Timestamp::Done();
  bool any_has_data = false;
  for (auto* mgr : managers_) {
    if (mgr->IsFull()) return kNotReady;
    bool empty;
    Timestamp ts = mgr->MinTimestampOrBound(&empty);
    if (!empty) any_has_data = true;
    *min_stream_timestamp = std::min(*min_stream_timestamp, ts);
  }
  return any_has_data ? kReadyForProcess : kNotReady;
}

void FixedSizeInputStreamHandler::FillInputSet(
    Timestamp timestamp, GraphContext& context) {
  for (auto* mgr : managers_) {
    int num_dropped;
    bool stream_is_done;
    mgr->PopPacketAtTimestamp(timestamp, &num_dropped, &stream_is_done);
  }
}

void FixedSizeInputStreamHandler::NotifyPacketArrival() {
  notified_ = true;
}

void FixedSizeInputStreamHandler::SetNextTimestampBound(
    CollectionItemId id, Timestamp bound) {
  if (id >= 0 && id < static_cast<int>(managers_.size())) {
    managers_[id]->SetNextTimestampBound(bound);
  }
}

absl::Status FixedSizeInputStreamHandler::AddPacketsToStream(
    CollectionItemId id, const std::list<Packet>& packets, bool* notify) {
  if (id < 0 || id >= static_cast<int>(managers_.size())) {
    return absl::InvalidArgumentError("Invalid stream id");
  }
  return managers_[id]->AddPackets(packets, notify);
}

absl::Status FixedSizeInputStreamHandler::MovePacketsToStream(
    CollectionItemId id, std::list<Packet>* packets, bool* notify) {
  if (id < 0 || id >= static_cast<int>(managers_.size())) {
    return absl::InvalidArgumentError("Invalid stream id");
  }
  return managers_[id]->MovePackets(packets, notify);
}

void FixedSizeInputStreamHandler::Close() {
  for (auto* mgr : managers_) mgr->Close();
}

// --- Factory ---

std::unique_ptr<InputStreamHandler> CreateInputStreamHandler(
    const std::string& name, int max_queue_size) {
  if (name == "immediate") {
    return std::make_unique<ImmediateInputStreamHandler>();
  }
  if (name == "fixed_size") {
    return std::make_unique<FixedSizeInputStreamHandler>(max_queue_size);
  }
  // "default", "sync_set", or any unrecognized name → SyncSet-based handler.
  return std::make_unique<DefaultInputStreamHandler>();
}

}  // namespace graph::runtime
