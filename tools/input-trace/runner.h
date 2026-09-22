#pragma once
#include "games/reference/reference.h"
#include "kernel/audio/audio.h"
#include "kernel/gameplay/session.h"
#include "platform/headless/headless.h"
#include "platform/headless/recorded_audio.h"
#include "sha256.h"
#include "trace.h"
#include <chrono>
#include <functional>
namespace tracecli {
struct Scratch {
  std::filesystem::path path;
  Scratch() {
    auto base = std::filesystem::temp_directory_path();
    auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    for (unsigned i = 0; i < 1000; ++i) {
      path = base /
             ("hrk-trace-" + std::to_string(seed) + "-" + std::to_string(i));
      std::error_code e;
      if (std::filesystem::create_directory(path, e))
        return;
    }
    throw Problem("TraceIoError", "Cannot create temporary directory", {}, {},
                  3);
  }
  ~Scratch() {
    std::error_code e;
    std::filesystem::remove_all(path, e);
  }
};
inline hrk::Error gameError(const std::string &name) {
  for (int i = 0; i <= static_cast<int>(hrk::Error::ReplayFull); ++i)
    if (name == hrk::errorName(static_cast<hrk::Error>(i)))
      return static_cast<hrk::Error>(i);
  throw Problem("TraceInvalidValue", "Unknown game error");
}
inline hrk::TraceOperation operation(const std::string &name) {
  const std::vector<std::string> names = {
      "load",           "start",          "pause",           "resume",
      "seek",           "stop",           "background",      "foreground",
      "surface_create", "surface_resize", "surface_destroy", "render",
      "status",         "update",         "measure",         "request_120hz",
      "unknown_command"};
  auto it = std::find(names.begin(), names.end(), name);
  if (it == names.end())
    throw Problem("TraceInvalidValue", "Unknown operation");
  return static_cast<hrk::TraceOperation>(it - names.begin());
}
inline float coordinate(const Json &j) {
  if (j.type == Json::Number)
    return static_cast<float>(std::stod(j.value));
  if (j.value == "NaN")
    return std::numeric_limits<float>::quiet_NaN();
  return j.value == "+Infinity" ? std::numeric_limits<float>::infinity()
                                : -std::numeric_limits<float>::infinity();
}
inline hrk::RawInputEvent raw(const Json &j) {
  return {static_cast<uint32_t>(uintValue(j.at("pointerId"), true)),
          static_cast<hrk::InputPhase>(uintValue(j.at("rawPhase"), true)),
          {coordinate(j.at("position").at("x")),
           coordinate(j.at("position").at("y"))},
          intValue(j.at("rawHostTime"))};
}
struct Run {
  Json result;
  Records trace;
};
inline Run runTrace(const Records &input, const std::string &chartBytes,
                    const std::string &audioBytes, bool verify = true) {
  using namespace hrk;
  validateTrace(input);
  // Raw event timestamps may be invalid evidence. Injected successful audio
  // values, however, must stay in the platform contract to avoid arithmetic UB.
  for (const auto &r : input) {
    if (r.at("kind").value != "clock" || !r.at("sampleSuccess").yes())
      continue;
    if (r.at("query").value == "position")
      insist(validTime(intValue(r.at("positionResult"))), "TraceInvalidValue",
             "Position outside platform range", r, "positionResult");
    else
      insist(intValue(r.at("sampleHostTime")) >= 0 &&
                 validTime(intValue(r.at("sampleSongTime"))),
             "TraceInvalidValue", "Clock sample outside platform range", r,
             "sampleSongTime");
  }
  insist(validTime(intValue(
             input.front().at("captureConfig").at("inputDeliveryGrace"))),
         "TraceInvalidValue", "Delivery grace outside platform range");
  const auto &provenance = input.front().at("provenance");
  insist(sha256(chartBytes) == provenance.at("chartSha256").value &&
             sha256(audioBytes) == provenance.at("audioSha256").value,
         "TraceAssetMismatch", "Chart/audio hash mismatch");
  RecordedAudioBackend audio(
      intValue(input.front().at("captureConfig").at("inputDeliveryGrace")));
  reference::ReferenceGame game;
  InputTrace trace;
  GameplaySession session(game, audio);
  session.setTrace(&trace);
  TracedPlatformQueue queue;
  queue.setTrace(&trace);
  NullRenderBackend render;
  std::shared_ptr<const IRuntimeChart> chart;
  Pcm pcm;
  try {
    chart = game.createChartLoader()->load(chartBytes);
    pcm = decodeWav(std::vector<uint8_t>(audioBytes.begin(), audioBytes.end()));
  } catch (const Failure &e) {
    throw Problem("TraceInvalidValue",
                  std::string("Asset decode: ") + e.what());
  }
  TraceHeader header;
  header.runId = input.front().at("runId").value;
  header.sourceRevision = provenance.at("sourceRevision").value;
  header.specRevision = provenance.at("specRevision").value;
  header.policyVersion = provenance.at("policyVersion").value;
  header.buildMode = provenance.at("buildMode").value;
  header.deviceModel = provenance.at("deviceModel").value;
  header.osVersion = provenance.at("osVersion").value;
  header.origin = provenance.at("origin").value;
  header.chartSha256 = provenance.at("chartSha256").value;
  header.audioSha256 = provenance.at("audioSha256").value;
  header.inputDeliveryGrace = audio.inputDeliveryGrace();
  insist(trace.beginCapture(header, false) == CaptureError::Ok, "TraceIoError",
         "Capture allocation failed");
  std::map<uint64_t, std::vector<const Json *>> actions;
  std::vector<const Json *> received;
  std::map<std::string, const Json *> ingress, platformTerminal;
  std::map<std::string, uint64_t> due;
  std::map<uint64_t, uint64_t> anchorAction;
  for (const auto &r : input) {
    if (r.object.count("source") && r.at("source").value == "session")
      actions[uintValue(r.at("actionSequence"))].push_back(&r);
    if (r.at("kind").value == "input") {
      auto id = r.at("eventSequence").value;
      if (r.at("stage").value == "receive") {
        ingress[id] = &r;
        received.push_back(&r);
      }
      if (r.at("source").value == "session" &&
          r.at("stage").value != "receive" && !due.count(id))
        due[id] = uintValue(r.at("actionSequence"));
      if (r.at("source").value == "platform" &&
          r.at("stage").value == "terminal")
        platformTerminal[id] = &r;
    }
    if (r.at("kind").value == "clock" && r.at("query").value == "anchor")
      anchorAction[uintValue(r.at("epoch"))] =
          uintValue(r.at("actionSequence"));
  }
  std::sort(received.begin(), received.end(), [](const Json *a, const Json *b) {
    return uintValue(a->at("eventSequence")) <
           uintValue(b->at("eventSequence"));
  });
  // Admit as late as possible, retaining ingress FIFO and epoch. This is a
  // queue- visibility equivalence, not an invented cross-thread physical
  // timestamp.
  const uint64_t last = actions.empty() ? 0 : actions.rbegin()->first;
  std::vector<uint64_t> admission(received.size(), last + 1);
  for (size_t i = 0; i < received.size(); ++i) {
    const auto &r = *received[i];
    auto id = r.at("eventSequence").value;
    if (due.count(id))
      admission[i] = due[id];
    if (r.at("source").value == "session")
      admission[i] = uintValue(r.at("actionSequence"));
    auto next = anchorAction.find(uintValue(r.at("epoch")) + 1);
    if (next != anchorAction.end() && r.at("source").value == "platform")
      admission[i] = std::min(admission[i], next->second);
  }
  // A full-queue rejection constrains the preceding capacity entries to remain
  // visible together. If that constraint conflicts with a poll, fail below.
  for (size_t i = 0; i < received.size(); ++i) {
    auto id = received[i]->at("eventSequence").value;
    if (platformTerminal.count(id) &&
        platformTerminal.at(id)->at("reason").value == "platform_queue_full") {
      size_t start = i > 2048 ? i - 2048 : 0;
      for (size_t j = start; j < i; ++j)
        admission[i] = std::min(admission[i], admission[j]);
    }
  }
  for (size_t i = admission.size(); i > 1; --i)
    admission[i - 2] = std::min(admission[i - 2], admission[i - 1]);
  std::map<uint64_t, std::string> eventIds;
  std::vector<std::pair<TraceSource, size_t>> terminalOrder;
  size_t scanned[2] = {};
  auto rememberTerminals = [&](TraceSource source) {
    auto &cursor = scanned[source == TraceSource::Platform ? 0 : 1];
    for (; cursor < trace.recordCount(source); ++cursor) {
      const auto &record = trace.record(source, cursor);
      if (record.kind == TraceKind::Input &&
          record.data.input.stage == TraceStage::Terminal)
        terminalOrder.emplace_back(source, cursor);
    }
  };
  size_t nextReceive = 0;
  auto admit = [&](uint64_t action) {
    while (nextReceive < received.size() && admission[nextReceive] <= action) {
      const auto &r = *received[nextReceive];
      if (r.at("source").value == "session")
        break;
      auto original = r.at("eventSequence").value;
      insist(uintValue(r.at("epoch")) == trace.epoch(),
             "TraceScheduleAmbiguous",
             "Cannot place ingress within observed epoch", r, "epoch");
      InputTrace::Writer writer(trace);
      bool invalid =
          platformTerminal.count(original) &&
          platformTerminal.at(original)->at("reason").value == "invalid_input";
      if (invalid)
        queue.rejectInvalid(
            raw(r), intValue(r.at("receiveHostTime")),
            r.at("batchSequence").null() ? 0 : uintValue(r.at("batchSequence")),
            r.at("pointIndex").null()
                ? 0
                : static_cast<uint32_t>(uintValue(r.at("pointIndex"), true)),
            true);
      else {
        bool ok = queue.submit(
            raw(r), intValue(r.at("receiveHostTime")),
            r.at("batchSequence").null() ? 0 : uintValue(r.at("batchSequence")),
            r.at("pointIndex").null()
                ? 0
                : static_cast<uint32_t>(uintValue(r.at("pointIndex"), true)),
            true);
        bool expected = !platformTerminal.count(original) ||
                        platformTerminal.at(original)->at("reason").value !=
                            "platform_queue_full";
        insist(ok == expected, "TraceScheduleAmbiguous",
               "Queue capacity/arrival interleaving cannot be established", r,
               "stage");
      }
      eventIds[nextReceive + 1] = original;
      ++nextReceive;
      rememberTerminals(TraceSource::Platform);
    }
  };
  for (const auto &ap : actions) {
    admit(ap.first);
    const auto &records = ap.second;
    std::vector<RecordedQuery> queries;
    Error control = Error::Ok;
    for (const auto *r : records) {
      if (r->at("kind").value == "clock" && r->at("query").value != "anchor") {
        RecordedQuery q;
        q.position = r->at("query").value == "position";
        q.success = r->at("sampleSuccess").yes();
        if (q.position)
          q.value = intValue(r->at("positionResult"));
        else if (q.success) {
          q.host = intValue(r->at("sampleHostTime"));
          q.song = intValue(r->at("sampleSongTime"));
        }
        queries.push_back(q);
      }
      if (r->at("kind").value == "control" &&
          r->at("boundary").value == "end" &&
          r->at("result").value == "BackendFailure")
        control = Error::BackendFailure;
    }
    audio.action(std::move(queries), control);
    std::function<size_t(size_t)> execute = [&](size_t begin) -> size_t {
      const auto &b = *records[begin];
      const auto kind = b.at("kind").value;
      size_t end = begin, depth = 0;
      for (; end < records.size(); ++end) {
        auto k = records[end]->at("kind").value;
        if (k != "control" && k != "update")
          continue;
        if (records[end]->at("boundary").value == "begin")
          ++depth;
        else if (--depth == 0)
          break;
      }
      insist(end < records.size(), "TraceIncomplete", "Missing end", b);
      const auto &e = *records[end];
      const auto name = kind == "update" ? "update" : b.at("operation").value;
      TimeNs host = intValue(b.at("hostTime"));
      bool wrapper = false;
      for (size_t i = begin + 1; i < end; ++i)
        if (records[i]->at("kind").value == "control" &&
            records[i]->at("boundary").value == "begin" &&
            records[i]->at("operation").value == name) {
          wrapper = true;
          break;
        }
      bool generic =
          wrapper ||
          oneOf(Json(name),
                "background foreground surface_create surface_resize "
                "surface_destroy measure request_120hz unknown_command");
      if (generic) {
        InputTrace::Scope scope(
            &trace, operation(name), host,
            name == "seek" ? intValue(b.at("targetSongTime")) : 0);
        for (size_t i = begin + 1; i < end;) {
          const auto &r = *records[i];
          if ((r.at("kind").value == "control" ||
               r.at("kind").value == "update") &&
              r.at("boundary").value == "begin")
            i = execute(i);
          else if (r.at("kind").value == "input" &&
                   r.at("stage").value == "terminal" &&
                   r.at("reason").value == "lifecycle_clear" &&
                   ingress.at(r.at("eventSequence").value)
                           ->at("source")
                           .value == "platform") {
            queue.clear();
            while (i < end && records[i]->at("kind").value == "input")
              ++i;
          } else
            ++i;
        }
        session.finishTrace(scope, gameError(e.at("result").value));
      } else if (name == "load")
        session.load(chart, pcm, RuntimeMode::Play);
      else if (name == "start")
        session.start(host);
      else if (name == "pause")
        session.pause();
      else if (name == "resume")
        session.resume(host);
      else if (name == "seek")
        session.seek(intValue(b.at("targetSongTime")), host);
      else if (name == "stop")
        session.stop();
      else if (name == "update")
        session.update();
      else if (name == "render")
        session.render(render, host);
      else if (name == "status")
        session.time();
      return end + 1;
    };
    const auto &first = *records.front();
    if (first.at("kind").value == "control" ||
        first.at("kind").value == "update")
      execute(0);
    else {
      InputTrace::Transfer transfer(&trace);
      for (const auto *p : records) {
        const auto &r = *p;
        if (r.at("kind").value != "input")
          continue;
        if (r.at("stage").value == "receive") {
          insist(nextReceive < received.size() && received[nextReceive] == p,
                 "TraceScheduleAmbiguous",
                 "Session ingress interleaves platform arrival ambiguously", r);
          auto env = trace.receive(raw(r), intValue(r.at("receiveHostTime")), 0,
                                   0, TraceSource::Session);
          eventIds[env.trace.event] = r.at("eventSequence").value;
          ++nextReceive;
          session.pushTraced(env);
        } else if (r.at("stage").value == "platform_poll") {
          InputEnvelope env;
          insist(queue.poll(env) &&
                     eventIds[env.trace.event] == r.at("eventSequence").value,
                 "TraceScheduleAmbiguous", "Poll differs from ingress FIFO", r,
                 "eventSequence");
          trace.platformPoll(env);
          if (session.state() == SessionState::Playing)
            session.pushTraced(env);
          else
            trace.cleared(env, TraceReason::InactiveDrain);
        }
      }
    }
    insist(audio.matched(), "TraceQueryMismatch",
           "Missing, unused or wrong public audio query", first, "query");
    rememberTerminals(TraceSource::Session);
  }
  admit(last + 1);
  insist(nextReceive == received.size(), "TraceScheduleAmbiguous",
         "Unscheduled ingress");
  // endCapture must preserve EOF pending; never pause merely to make export
  // possible.
  insist(trace.endCapture(session.state() == SessionState::Playing) ==
             CaptureError::Ok,
         "TraceIncomplete", "Trace ends while playing");
  Scratch temp;
  auto file = temp.path / "actual.jsonl";
  insist(trace.exportCapture(file.string()) == CaptureError::Ok, "TraceIoError",
         "Cannot export replay trace");
  auto actual = readTrace(file);
  validateTrace(actual);
  for (auto &r : actual)
    if (r.at("kind").value == "input")
      r.at("eventSequence") = eventIds.at(uintValue(r.at("eventSequence")));
  for (auto &id : actual.back().at("pendingEventSequences").array)
    id = eventIds.at(uintValue(id));
  if (verify) {
    insist(actual.size() == input.size(), "TraceTargetNotReproduced",
           "Observed record count differs");
    for (size_t i = 1; i + 1 < input.size(); ++i) {
      auto expected = input[i], observed = actual[i];
      if (expected.at("kind").value == "input" &&
          observed.at("kind").value == "input") {
        for (const auto &axis : {"x", "y"}) {
          auto a = coordinate(expected.at("position").at(axis));
          auto b = coordinate(observed.at("position").at(axis));
          if (a == b || (std::isnan(a) && std::isnan(b)))
            observed.at("position").at(axis) = expected.at("position").at(axis);
        }
      }
      for (const auto &k : {"hostTime", "consumeHostTime"})
        if (expected.object.count(k))
          observed.at(k) = expected.at(k);
      auto diff = compareResults(expected, observed);
      insist(diff.at("equal").yes(), "TraceTargetNotReproduced",
             "Observed behavior differs: " + dump(diff), input[i]);
    }
    insist(actual.back() == input.back(), "TraceTargetNotReproduced",
           "Summary differs");
  }
  Json result = Json::obj(
      {{"complete", true},
       {"diagnosticError", Json()},
       {"decisions", Json::list()},
       {"judgments", Json::list()},
       {"score", Json::obj()},
       {"boundaries", Json::list()},
       {"pendingEventSequences", actual.back().at("pendingEventSequences")}});
  for (const auto &entry : terminalOrder) {
    const auto &r = actual.at(1 + entry.second +
                              (entry.first == TraceSource::Session
                                   ? trace.recordCount(TraceSource::Platform)
                                   : 0));
    result.at("decisions")
        .array.push_back(Json::obj({{"eventSequence", r.at("eventSequence")},
                                    {"disposition", r.at("disposition")},
                                    {"reason", r.at("reason")},
                                    {"error", r.at("error")}}));
  }
  for (const auto &r : actual) {
    if ((r.at("kind").value == "control" || r.at("kind").value == "update") &&
        r.at("boundary").value == "end")
      result.at("boundaries")
          .array.push_back(
              Json::obj({{"actionSequence", r.at("actionSequence")},
                         {"recordSequence", r.at("recordSequence")},
                         {"activePointers", r.at("activePointers")}}));
  }
  for (const auto &j : session.judgments())
    result.at("judgments")
        .array.push_back(
            Json::obj({{"entity", std::to_string(j.entity)},
                       {"type", Json::number(j.type)},
                       {"targetTime", std::to_string(j.targetTime)},
                       {"inputTime", std::to_string(j.inputTime)},
                       {"error", std::to_string(j.error)}}));
  auto score = session.score();
  result.at("score") = Json::obj({{"value", std::to_string(score.value)},
                                  {"perfect", Json::number(score.perfect)},
                                  {"miss", Json::number(score.miss)}});
  return {result, actual};
}
} // namespace tracecli
