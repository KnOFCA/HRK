#pragma once
#include "json.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
namespace tracecli {
using Records = std::vector<Json>;
inline void insist(bool yes, const std::string &code,
                   const std::string &message, const Json &r = Json(),
                   const std::string &field = {}) {
  if (!yes)
    throw Problem(code, message,
                  r.type == Json::Object && r.object.count("recordSequence")
                      ? r.at("recordSequence").value
                      : "",
                  field);
}
inline std::string readFile(const std::filesystem::path &p) {
  std::error_code ec;
  auto n = std::filesystem::file_size(p, ec);
  if (ec)
    throw Problem("TraceIoError", "Cannot read " + p.string(), {}, {}, 3);
  if (n > 512ULL * 1024 * 1024)
    throw Problem("TraceLimitExceeded", "File exceeds 512 MiB");
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw Problem("TraceIoError", "Cannot open input", {}, {}, 3);
  std::string s(static_cast<size_t>(n), '\0');
  f.read(s.data(), static_cast<std::streamsize>(n));
  if (!f)
    throw Problem("TraceIoError", "Short read", {}, {}, 3);
  return s;
}
inline Records readTrace(const std::filesystem::path &p) {
  std::error_code ec;
  auto n = std::filesystem::file_size(p, ec);
  if (ec)
    throw Problem("TraceIoError", "Cannot stat trace", {}, {}, 3);
  if (n > 512ULL * 1024 * 1024)
    throw Problem("TraceLimitExceeded", "File exceeds 512 MiB");
  std::ifstream f(p, std::ios::binary);
  if (!f)
    throw Problem("TraceIoError", "Cannot open trace", {}, {}, 3);
  Records out;
  std::string line;
  char c;
  auto append = [&] {
    try {
      insist(!line.empty(), "TraceInvalidSchema", "Empty JSONL line");
      out.push_back(Parser(line).parse());
      line.clear();
    } catch (Problem &e) {
      if (e.record.empty())
        e.record = std::to_string(out.size() + 1);
      throw;
    }
  };
  while (f.get(c)) {
    if (c == '\n')
      append();
    else {
      if (line.size() >= 1024 * 1024)
        throw Problem("TraceLimitExceeded", "Line exceeds 1 MiB",
                      std::to_string(out.size() + 1));
      line += c;
    }
  }
  if (!f.eof())
    throw Problem("TraceIoError", "Trace read failed", {}, {}, 3);
  if (!line.empty())
    append();
  insist(!out.empty(), "TraceInvalidSchema", "Empty trace");
  return out;
}
inline void keys(const Json &j, const std::string &fields) {
  std::istringstream in(fields);
  std::set<std::string> expected;
  std::string k;
  while (in >> k)
    expected.insert(k);
  insist(j.type == Json::Object, "TraceInvalidSchema", "Expected object", j);
  for (const auto &p : j.object)
    insist(expected.erase(p.first) != 0, "TraceInvalidSchema", "Unknown key", j,
           p.first);
  insist(expected.empty(), "TraceInvalidSchema", "Missing key", j,
         expected.empty() ? "" : *expected.begin());
}
inline bool oneOf(const Json &j, const std::string &choices) {
  if (j.type != Json::String)
    return false;
  std::istringstream in(choices);
  std::string s;
  while (in >> s)
    if (j.value == s)
      return true;
  return false;
}
inline void enumeration(const Json &r, const std::string &key,
                        const std::string &choices, bool nullable = false) {
  insist((nullable && r.at(key).null()) || oneOf(r.at(key), choices),
         "TraceInvalidValue", "Unknown enum", r, key);
}
inline const std::string errors =
    "Ok InvalidArgument InvalidState ChartParseError ResourceNotFound "
    "AudioInvalid BackendFailure UnsupportedVersion ReplayInvalid "
    "ReplayChartMismatch ReplayIncompatible QueueFull StaleInput LateInput "
    "ReplayFull";
inline const std::string reasons =
    "before_epoch mapped_out_of_range before_watermark platform_queue_full "
    "session_queue_full invalid_input invalid_state replay_full "
    "lifecycle_clear inactive_drain";
inline const std::string operations =
    "load start pause resume seek stop background foreground surface_create "
    "surface_resize surface_destroy render status measure request_120hz "
    "unknown_command";
inline const std::string diagnostics =
    "TraceInvalidSchema TraceUnsupportedVersion TraceInvalidValue "
    "TraceMissingReference TraceIncomplete TraceScheduleAmbiguous "
    "TraceQueryMismatch TraceAssetMismatch TraceLimitExceeded "
    "TraceTargetNotReproduced CaptureInvalidState CaptureAllocationFailed "
    "CaptureFull CaptureCounterOverflow CaptureConcurrencyUnsupported "
    "CaptureBusy CaptureExportFailed TraceIoError";
inline void strings(const Json &j) {
  if (j.type == Json::String)
    insist(j.value.size() <= 256, "TraceLimitExceeded",
           "String exceeds 256 bytes");
  for (const auto &a : j.array)
    strings(a);
  for (const auto &p : j.object)
    strings(p.second);
}
inline void schema(const Json &r) {
  strings(r);
  const auto kind = r.at("kind").str();
  const std::string common = "kind recordSequence ",
                    runtime = "source sourceSequence epoch ";
  if (kind == "header")
    keys(r, common + "traceVersion runId provenance captureConfig");
  else if (kind == "input")
    keys(r, common + runtime +
                "eventSequence batchSequence pointIndex pointerId phase "
                "position rawHostTime receiveHostTime consumeHostTime "
                "mappedSongTime mappingSampleSequence watermarkBefore "
                "watermarkAfter songNow stage disposition error reason "
                "rawPhase actionSequence controlSequence updateSequence");
  else if (kind == "clock")
    keys(r, common + runtime +
                "sampleHostTime sampleSongTime sampleSuccess query caller "
                "actionSequence queryIndex positionResult");
  else if (kind == "control")
    keys(r, common + runtime +
                "operation hostTime targetSongTime result actionSequence "
                "boundary activePointers");
  else if (kind == "update")
    keys(r, common + runtime +
                "updateSequence hostTime watermarkBefore watermarkAfter "
                "songNow actionSequence boundary result activePointers");
  else if (kind == "summary")
    keys(r, common +
                "countsByPhaseAndReason captureDropped pendingAtEnd complete "
                "ingressCount acceptedCount rejectedCount clearedCount "
                "captureDroppedBySource platformQueueDropped "
                "sessionQueueDropped pendingEventSequences incompleteReasons");
  else
    throw Problem("TraceInvalidSchema", "Unknown record kind", {}, "kind");
  const std::set<std::string> us = {
      "recordSequence",       "sourceSequence",     "epoch",
      "eventSequence",        "batchSequence",      "mappingSampleSequence",
      "actionSequence",       "controlSequence",    "updateSequence",
      "captureDropped",       "pendingAtEnd",       "ingressCount",
      "acceptedCount",        "rejectedCount",      "clearedCount",
      "platformQueueDropped", "sessionQueueDropped"};
  const std::set<std::string> is = {
      "rawHostTime",    "receiveHostTime", "consumeHostTime",
      "mappedSongTime", "watermarkBefore", "watermarkAfter",
      "songNow",        "sampleHostTime",  "sampleSongTime",
      "positionResult", "hostTime",        "targetSongTime"};
  const std::set<std::string> nullable = {
      "batchSequence",   "pointIndex",      "mappingSampleSequence",
      "actionSequence",  "controlSequence", "updateSequence",
      "consumeHostTime", "mappedSongTime",  "watermarkBefore",
      "watermarkAfter",  "songNow",         "sampleHostTime",
      "sampleSongTime",  "positionResult",  "targetSongTime"};
  for (const auto &p : r.object) {
    try {
      if (us.count(p.first) || is.count(p.first)) {
        if (p.second.null()) {
          insist(nullable.count(p.first) != 0, "TraceInvalidSchema",
                 "Unexpected null", r, p.first);
          continue;
        }
        if (us.count(p.first)) {
          auto n = uintValue(p.second);
          if (p.first.find("Sequence") != std::string::npos)
            insist(n != 0, "TraceInvalidValue", "Zero sequence", r, p.first);
        } else
          intValue(p.second);
      }
    } catch (Problem &e) {
      e.field = p.first;
      throw;
    }
  }
  for (const auto &k : {"pointerId", "rawPhase", "pointIndex", "queryIndex"})
    if (r.object.count(k) && !r.at(k).null())
      insist(uintValue(r.at(k), true) <= UINT32_MAX, "TraceInvalidValue",
             "u32 out of range", r, k);
  if (kind == "header") {
    insist(r.at("traceVersion") == Json::number(1), "TraceUnsupportedVersion",
           "Unsupported trace version", r, "traceVersion");
    insist(!r.at("runId").str().empty(), "TraceInvalidValue", "Empty runId", r,
           "runId");
    const auto &p = r.at("provenance");
    keys(p, "sourceRevision specRevision policyVersion buildMode deviceModel "
            "osVersion actualRefreshHz chartSha256 audioSha256 origin");
    for (const auto &k : {"sourceRevision", "specRevision", "policyVersion",
                          "deviceModel", "osVersion"})
      insist(!p.at(k).str().empty(), "TraceInvalidValue", "Empty provenance", r,
             k);
    enumeration(p, "buildMode", "Debug Release");
    enumeration(p, "origin", "human injected synthetic");
    for (const auto &k : {"chartSha256", "audioSha256"}) {
      auto h = p.at(k).str();
      insist(h.size() == 64 &&
                 h.find_first_not_of("0123456789abcdef") == std::string::npos,
             "TraceInvalidValue", "Invalid SHA256", r, k);
    }
    const auto &hz = p.at("actualRefreshHz");
    insist(hz.null() || (hz.type == Json::Number && std::stod(hz.value) > 0),
           "TraceInvalidValue", "Invalid refresh rate", r, "actualRefreshHz");
    const auto &c = r.at("captureConfig");
    keys(c, "enabled platformCapacity sessionCapacity slotBytes "
            "memoryBudgetBytes hostClock timeUnit inputDeliveryGrace policy");
    insist(c.at("enabled").yes() &&
               uintValue(c.at("platformCapacity"), true) == 65536 &&
               uintValue(c.at("sessionCapacity"), true) == 65536 &&
               uintValue(c.at("slotBytes"), true) > 0 &&
               uintValue(c.at("slotBytes"), true) <= 256 &&
               uintValue(c.at("memoryBudgetBytes"), true) == 34603008,
           "TraceInvalidValue", "Capture budget mismatch", r, "captureConfig");
    enumeration(c, "hostClock", "CLOCK_MONOTONIC");
    enumeration(c, "timeUnit", "ns");
    enumeration(c, "policy", "v0.1.0-observe");
    insist(intValue(c.at("inputDeliveryGrace")) >= 0, "TraceInvalidValue",
           "Negative grace", r, "inputDeliveryGrace");
  } else if (kind == "input") {
    enumeration(r, "phase", "DOWN MOVE UP CANCEL UNKNOWN");
    enumeration(r, "stage",
                "receive platform_enqueue platform_poll session_enqueue map "
                "pending terminal");
    enumeration(r, "disposition",
                "observed queued pending accepted rejected cleared");
    enumeration(r, "error", errors);
    enumeration(r, "reason", reasons, true);
    keys(r.at("position"), "x y");
    for (const auto &p : r.at("position").object)
      insist(p.second.type == Json::Number ||
                 oneOf(p.second, "NaN +Infinity -Infinity"),
             "TraceInvalidValue", "Invalid coordinate", r, "position");
    const auto phase = uintValue(r.at("rawPhase"), true);
    const char *phases[] = {"DOWN", "MOVE", "UP", "CANCEL"};
    insist(r.at("phase").value == (phase < 4 ? phases[phase] : "UNKNOWN"),
           "TraceInvalidValue", "Phase mismatch", r, "phase");
    insist(r.at("batchSequence").null() == r.at("pointIndex").null(),
           "TraceInvalidSchema", "Batch/point null mismatch", r, "pointIndex");
    const auto stage = r.at("stage").value;
    const auto disp = r.at("disposition").value;
    insist(stage == "terminal"
               ? oneOf(r.at("disposition"), "accepted rejected cleared")
               : disp == (stage == "pending" ? "pending"
                          : (stage == "platform_enqueue" ||
                             stage == "session_enqueue")
                              ? "queued"
                              : "observed"),
           "TraceInvalidValue", "Stage/disposition mismatch", r, "disposition");
    if (disp == "accepted" || stage != "terminal")
      insist(r.at("reason").null() && r.at("error").value == "Ok",
             "TraceInvalidValue", "Unexpected stage error", r, "reason");
    if (stage == "terminal" && disp != "accepted")
      insist(!r.at("reason").null(), "TraceInvalidValue",
             "Missing terminal reason", r, "reason");
  } else if (kind == "clock") {
    enumeration(r, "query", "position clockSample anchor");
    enumeration(r, "caller", "start resume seek update render status pause");
    auto success = r.at("sampleSuccess").yes();
    bool pos = r.at("query").value == "position";
    insist(pos ? (success && !r.at("positionResult").null() &&
                  r.at("sampleHostTime").null() &&
                  r.at("sampleSongTime").null())
               : (r.at("positionResult").null() &&
                  r.at("sampleHostTime").null() != success &&
                  r.at("sampleSongTime").null() != success),
           "TraceInvalidSchema", "Clock null pattern", r, "sampleSuccess");
    if (r.at("query").value == "anchor")
      insist(success, "TraceInvalidValue", "Failed anchor", r, "sampleSuccess");
  } else if (kind == "control" || kind == "update") {
    enumeration(r, "boundary", "begin end");
    enumeration(r, "result", errors, true);
    if (kind == "control") {
      enumeration(r, "operation", operations);
      insist(r.at("operation").value == "seek" ? !r.at("targetSongTime").null()
                                               : r.at("targetSongTime").null(),
             "TraceInvalidSchema", "Target only for seek", r, "targetSongTime");
    }
    if (r.at("boundary").value == "begin")
      insist(r.at("result").null() && r.at("activePointers").null(),
             "TraceInvalidSchema", "Invalid begin", r, "result");
    else {
      insist(!r.at("result").null() &&
                 r.at("activePointers").type == Json::Array,
             "TraceInvalidSchema", "Invalid end", r, "activePointers");
      uint64_t prev = 0;
      bool first = true;
      for (const auto &id : r.at("activePointers").array) {
        auto n = uintValue(id, true);
        insist(n <= UINT32_MAX && (first || n > prev), "TraceInvalidValue",
               "Pointers not unique/sorted", r, "activePointers");
        prev = n;
        first = false;
      }
    }
  } else if (kind == "summary") {
    r.at("complete").yes();
    keys(r.at("captureDroppedBySource"), "platform session");
    for (const auto &p : r.at("captureDroppedBySource").object)
      uintValue(p.second);
    for (const auto &k : {"countsByPhaseAndReason", "pendingEventSequences",
                          "incompleteReasons"})
      insist(r.at(k).type == Json::Array, "TraceInvalidSchema",
             "Expected array", r, k);
    std::set<std::string> seen;
    for (const auto &p : r.at("countsByPhaseAndReason").array) {
      keys(p, "phase disposition reason count");
      enumeration(p, "phase", "DOWN MOVE UP CANCEL UNKNOWN");
      enumeration(p, "disposition", "accepted rejected cleared");
      enumeration(p, "reason", reasons, true);
      insist(uintValue(p.at("count")) > 0 &&
                 seen.insert(dump(p.at("phase")) + dump(p.at("disposition")) +
                             dump(p.at("reason")))
                     .second,
             "TraceInvalidValue", "Duplicate/zero count", r,
             "countsByPhaseAndReason");
    }
    seen.clear();
    for (const auto &id : r.at("pendingEventSequences").array)
      insist(uintValue(id) > 0 && seen.insert(id.value).second,
             "TraceInvalidValue", "Invalid pending id", r,
             "pendingEventSequences");
    seen.clear();
    for (const auto &e : r.at("incompleteReasons").array)
      insist(oneOf(e, diagnostics) && seen.insert(e.value).second,
             "TraceInvalidValue", "Invalid incomplete reason", r,
             "incompleteReasons");
  }
  if (kind != "header" && kind != "summary") {
    enumeration(r, "source", "platform session");
    if (r.at("source").value == "platform") {
      insist(kind == "input", "TraceInvalidSchema", "Platform non-input", r,
             "source");
      for (const auto &k :
           {"actionSequence", "controlSequence", "updateSequence",
            "consumeHostTime", "mappedSongTime", "mappingSampleSequence",
            "watermarkBefore", "watermarkAfter", "songNow"})
        insist(r.at(k).null(), "TraceInvalidSchema",
               "Platform value must be null", r, k);
    } else
      insist(!r.at("actionSequence").null(), "TraceMissingReference",
             "Missing action", r, "actionSequence");
  }
}
inline void validateTrace(const Records &records) {
  insist(records.size() >= 2, "TraceIncomplete", "Missing header/summary");
  for (size_t i = 0; i < records.size(); ++i) {
    try {
      schema(records[i]);
      insist(uintValue(records[i].at("recordSequence")) == i + 1,
             "TraceInvalidValue", "Non-contiguous records", records[i],
             "recordSequence");
    } catch (Problem &e) {
      if (e.record.empty())
        e.record = std::to_string(i + 1);
      throw;
    }
  }
  insist(records.front().at("kind").value == "header" &&
             records.back().at("kind").value == "summary",
         "TraceIncomplete", "Missing header/summary");
  const auto &summary = records.back();
  insist(summary.at("complete").yes() &&
             summary.at("captureDropped").value == "0" &&
             summary.at("incompleteReasons").array.empty(),
         "TraceIncomplete", "Capture is incomplete", summary);
  insist(uintValue(summary.at("captureDroppedBySource").at("platform")) == 0 &&
             uintValue(summary.at("captureDroppedBySource").at("session")) == 0,
         "TraceIncomplete", "Dropped records", summary);
  std::map<std::string, const Json *> ingress, terminal, mapped;
  std::map<std::string, std::set<std::string>> stages;
  std::map<std::string, uint64_t> counts;
  std::vector<const Json *> stack;
  std::map<std::string, uint64_t> queries;
  std::map<std::string, uint64_t> batchPoints;
  uint64_t src[2] = {}, action = 0, epoch = 0, update = 0;
  bool session = false;
  auto ref = [&](const Json &r, const char *key) -> const Json & {
    auto n = uintValue(r.at(key));
    insist(n > 0 && n <= records.size(), "TraceMissingReference",
           "Reference out of range", r, key);
    return records[static_cast<size_t>(n - 1)];
  };
  for (size_t index = 1; index + 1 < records.size(); ++index) {
    const auto &r = records[index];
    auto kind = r.at("kind").value;
    insist(kind != "header" && kind != "summary", "TraceInvalidSchema",
           "Extra header/summary", r);
    bool platform = r.at("source").value == "platform";
    if (!platform)
      session = true;
    else
      insist(!session, "TraceInvalidValue", "Platform after Session", r,
             "source");
    insist(uintValue(r.at("sourceSequence")) == ++src[platform ? 0 : 1],
           "TraceIncomplete", "Source gap", r, "sourceSequence");
    if (!platform) {
      auto a = uintValue(r.at("actionSequence"));
      insist(a == action || a == action + 1, "TraceIncomplete",
             "Action gap/order", r, "actionSequence");
      action = a;
      if (kind == "clock" && r.at("query").value == "anchor")
        ++epoch;
      insist(uintValue(r.at("epoch")) == epoch, "TraceMissingReference",
             "Epoch mismatch", r, "epoch");
    }
    if (kind == "clock") {
      insist(uintValue(r.at("queryIndex"), true) ==
                 queries[r.at("actionSequence").value]++,
             "TraceQueryMismatch", "Query index/order", r, "queryIndex");
    }
    if (kind == "control" || kind == "update") {
      if (r.at("boundary").value == "begin") {
        if (kind == "update")
          insist(uintValue(r.at("updateSequence")) == ++update,
                 "TraceIncomplete", "Update gap", r, "updateSequence");
        stack.push_back(&r);
      } else {
        insist(!stack.empty(), "TraceIncomplete", "Unpaired end", r,
               "boundary");
        auto b = stack.back();
        stack.pop_back();
        insist(b->at("kind") == r.at("kind") &&
                   b->at("actionSequence") == r.at("actionSequence") &&
                   (kind == "control"
                        ? b->at("operation") == r.at("operation")
                        : b->at("updateSequence") == r.at("updateSequence")),
               "TraceIncomplete", "Boundary mismatch", r, "boundary");
      }
      continue;
    }
    if (kind != "input")
      continue;
    const auto id = r.at("eventSequence").value, stage = r.at("stage").value;
    if (stage == "receive")
      insist(ingress.emplace(id, &r).second, "TraceIncomplete",
             "Duplicate receive", r, "eventSequence");
    if (stage == "receive" && !r.at("batchSequence").null()) {
      const auto batch = r.at("batchSequence").value;
      auto point = uintValue(r.at("pointIndex"), true);
      insist(!batchPoints.count(batch) || point > batchPoints.at(batch),
             "TraceIncomplete", "Batch point order is not increasing", r,
             "pointIndex");
      batchPoints[batch] = point;
    }
    insist(ingress.count(id) != 0, "TraceMissingReference", "Missing receive",
           r, "eventSequence");
    insist(terminal.count(id) == 0, "TraceIncomplete", "Record after terminal",
           r, "stage");
    for (const auto &k :
         {"batchSequence", "pointIndex", "pointerId", "phase", "rawPhase",
          "position", "rawHostTime", "receiveHostTime"})
      insist(r.at(k) == ingress.at(id)->at(k), "TraceMissingReference",
             "Raw event changed", r, k);
    if (stage == "platform_enqueue")
      insist(platform && stages[id].count("receive"), "TraceIncomplete",
             "Enqueue before receive", r, "stage");
    if (stage == "platform_poll")
      insist(!platform && stages[id].count("platform_enqueue"),
             "TraceIncomplete", "Poll without enqueue", r, "stage");
    if (stage == "session_enqueue" &&
        ingress.at(id)->at("source").value == "platform")
      insist(stages[id].count("platform_poll") != 0, "TraceIncomplete",
             "Session enqueue without poll", r, "stage");
    if (stage == "map" || stage == "pending")
      insist(stages[id].count("session_enqueue") != 0, "TraceIncomplete",
             "Missing Session enqueue", r, "stage");
    if (stage == "pending" || r.at("disposition").value == "accepted" ||
        r.at("reason").value == "before_watermark" ||
        r.at("reason").value == "replay_full")
      insist(stages[id].count("map") != 0, "TraceIncomplete",
             "Missing successful mapping stage", r, "stage");
    if (stage == "map" || stage == "pending" ||
        r.at("disposition").value == "accepted")
      for (const auto &key :
           {"consumeHostTime", "mappedSongTime", "mappingSampleSequence"})
        insist(!r.at(key).null(), "TraceMissingReference",
               "Missing mapped value", r, key);
    if (stage != "pending" && stage != "terminal")
      insist(stages[id].insert(stage).second, "TraceIncomplete",
             "Duplicate stage", r, "stage");
    if (!r.at("mappingSampleSequence").null()) {
      const auto &c = ref(r, "mappingSampleSequence");
      insist(c.at("kind").value == "clock" && c.at("sampleSuccess").yes() &&
                 c.at("query").value != "position" &&
                 uintValue(c.at("recordSequence")) <
                     uintValue(r.at("recordSequence")) &&
                 uintValue(c.at("actionSequence")) <=
                     uintValue(r.at("actionSequence")),
             "TraceMissingReference", "Invalid sample reference", r,
             "mappingSampleSequence");
      if (mapped.count(id)) {
        for (const auto &k :
             {"mappedSongTime", "mappingSampleSequence", "consumeHostTime"})
          insist(r.at(k) == mapped.at(id)->at(k), "TraceMissingReference",
                 "Pending remapped", r, k);
      } else if (!r.at("mappedSongTime").null())
        mapped[id] = &r;
    }
    if (!r.at("controlSequence").null()) {
      const auto &c = ref(r, "controlSequence");
      insist(c.at("kind").value == "control" &&
                 c.at("boundary").value == "begin" &&
                 c.at("actionSequence") == r.at("actionSequence"),
             "TraceMissingReference", "Invalid control reference", r,
             "controlSequence");
    }
    if (!r.at("updateSequence").null())
      insist(std::any_of(stack.begin(), stack.end(),
                         [&](const Json *b) {
                           return b->at("kind").value == "update" &&
                                  b->at("updateSequence") ==
                                      r.at("updateSequence");
                         }),
             "TraceMissingReference", "Invalid update reference", r,
             "updateSequence");
    if (stage == "terminal") {
      terminal[id] = &r;
      counts[r.at("disposition").value]++;
      counts[dump(r.at("phase")) + dump(r.at("disposition")) +
             dump(r.at("reason"))]++;
      if (!r.at("reason").null())
        counts[r.at("reason").value]++;
      if (r.at("phase").value == "UNKNOWN")
        insist(r.at("reason").value == "invalid_input", "TraceInvalidValue",
               "UNKNOWN not rejected invalid", r, "reason");
    }
  }
  insist(stack.empty(), "TraceIncomplete", "Missing end boundary");
  std::set<std::string> pending;
  for (const auto &p : ingress)
    if (!terminal.count(p.first))
      pending.insert(p.first);
  std::set<std::string> expected;
  for (const auto &p : summary.at("pendingEventSequences").array)
    expected.insert(p.value);
  insist(pending == expected &&
             uintValue(summary.at("pendingAtEnd")) == pending.size() &&
             uintValue(summary.at("ingressCount")) == ingress.size(),
         "TraceIncomplete", "Ingress/pending accounting", summary);
  for (const auto &d : {"accepted", "rejected", "cleared"})
    insist(uintValue(summary.at(std::string(d) + "Count")) == counts[d],
           "TraceIncomplete", "Terminal accounting", summary,
           std::string(d) + "Count");
  insist(uintValue(summary.at("platformQueueDropped")) ==
                 counts["platform_queue_full"] &&
             uintValue(summary.at("sessionQueueDropped")) ==
                 counts["session_queue_full"],
         "TraceIncomplete", "Queue accounting", summary);
  size_t combinations = 0;
  for (const auto &p : counts)
    if (!p.first.empty() && p.first[0] == '"')
      ++combinations;
  insist(summary.at("countsByPhaseAndReason").array.size() == combinations,
         "TraceIncomplete", "Missing phase counts", summary);
  for (const auto &c : summary.at("countsByPhaseAndReason").array)
    insist(uintValue(c.at("count")) ==
               counts[dump(c.at("phase")) + dump(c.at("disposition")) +
                      dump(c.at("reason"))],
           "TraceIncomplete", "Phase accounting", summary);
}
inline Json compareResults(const Json &a, const Json &b,
                           const std::string &path = "$") {
  if (a == b)
    return Json::obj({{"equal", true}});
  if (a.type == b.type && a.type == Json::Object &&
      a.object.size() == b.object.size()) {
    for (const auto &p : a.object) {
      auto it = b.object.find(p.first);
      if (it == b.object.end())
        break;
      if (p.second != it->second)
        return compareResults(p.second, it->second, path + "." + p.first);
    }
  }
  if (a.type == b.type && a.type == Json::Array) {
    size_t n = std::min(a.array.size(), b.array.size());
    for (size_t i = 0; i < n; ++i)
      if (a.array[i] != b.array[i])
        return compareResults(a.array[i], b.array[i],
                              path + "[" + std::to_string(i) + "]");
    if (a.array.size() != b.array.size())
      return Json::obj({{"equal", false},
                        {"path", path + ".length"},
                        {"expected", Json::number(a.array.size())},
                        {"actual", Json::number(b.array.size())}});
  }
  return Json::obj(
      {{"equal", false}, {"path", path}, {"expected", a}, {"actual", b}});
}
} // namespace tracecli
