#include "graph_runtime/src/scheduler/input_stream_handler.h"

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
      FillInputSet(min_ts, context);
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

void DefaultInputStreamHandler::Close() {
  for (auto* mgr : managers_) {
    mgr->Close();
  }
}

}  // namespace graph::runtime
