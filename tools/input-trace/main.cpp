#include "runner.h"
#include <iostream>
namespace tracecli {
void writeNew(const std::filesystem::path &destination,
              const std::string &bytes) {
  if (destination.empty() || std::filesystem::exists(destination))
    throw Problem("TraceIoError", "Output already exists or is empty", {}, {},
                  3);
  auto parent = destination.parent_path();
  if (parent.empty())
    parent = ".";
  std::filesystem::path staging;
  bool created = false;
  auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
  for (unsigned i = 0; i < 1000; ++i) {
    staging = parent /
              (".hrk-trace-" + std::to_string(seed) + "-" + std::to_string(i));
    std::error_code ec;
    if (std::filesystem::create_directory(staging, ec)) {
      created = true;
      break;
    }
  }
  if (!created)
    throw Problem("TraceIoError", "Cannot create output staging directory", {},
                  {}, 3);
  try {
    auto file = staging / "payload";
    {
      std::ofstream f(file, std::ios::binary);
      if (!f)
        throw Problem("TraceIoError", "Cannot open output", {}, {}, 3);
      f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
      f.close();
      if (!f)
        throw Problem("TraceIoError", "Cannot write output", {}, {}, 3);
    }
    std::error_code ec;
    std::filesystem::create_hard_link(file, destination, ec);
    if (ec)
      throw Problem("TraceIoError", "Cannot publish output without overwriting",
                    {}, {}, 3);
    std::filesystem::remove_all(staging);
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove_all(staging, ec);
    throw;
  }
}
std::string jsonl(const Records &records) {
  std::string s;
  for (const auto &r : records)
    s += dump(r) + "\n";
  return s;
}
void recount(Records &records) {
  auto &s = records.back();
  std::set<std::string> events;
  std::map<std::string, Json> counts;
  uint64_t accepted = 0, rejected = 0, cleared = 0, pq = 0, sq = 0;
  for (const auto &r : records)
    if (r.at("kind").value == "input") {
      auto id = r.at("eventSequence").value;
      if (r.at("stage").value == "receive")
        events.insert(id);
      if (r.at("stage").value == "terminal") {
        events.erase(id);
        auto d = r.at("disposition").value;
        if (d == "accepted")
          ++accepted;
        else if (d == "rejected")
          ++rejected;
        else
          ++cleared;
        if (r.at("reason").value == "platform_queue_full")
          ++pq;
        if (r.at("reason").value == "session_queue_full")
          ++sq;
        auto key = dump(r.at("phase")) + dump(r.at("disposition")) +
                   dump(r.at("reason"));
        if (!counts.count(key))
          counts[key] = Json::obj({{"phase", r.at("phase")},
                                   {"disposition", r.at("disposition")},
                                   {"reason", r.at("reason")},
                                   {"count", "0"}});
        auto &c = counts[key].at("count");
        c = std::to_string(uintValue(c) + 1);
      }
    }
  s.at("ingressCount") =
      std::to_string(accepted + rejected + cleared + events.size());
  s.at("acceptedCount") = std::to_string(accepted);
  s.at("rejectedCount") = std::to_string(rejected);
  s.at("clearedCount") = std::to_string(cleared);
  s.at("pendingAtEnd") = std::to_string(events.size());
  s.at("platformQueueDropped") = std::to_string(pq);
  s.at("sessionQueueDropped") = std::to_string(sq);
  s.at("countsByPhaseAndReason") = Json::list();
  for (const auto &p : counts)
    s.at("countsByPhaseAndReason").array.push_back(p.second);
  s.at("pendingEventSequences") = Json::list();
  std::vector<std::string> ids(events.begin(), events.end());
  std::sort(ids.begin(), ids.end(), [](const auto &a, const auto &b) {
    return uintValue(Json(a)) < uintValue(Json(b));
  });
  for (const auto &id : ids)
    s.at("pendingEventSequences").array.emplace_back(id);
}
Records subset(const Records &source, const std::string &event,
               const std::string &action,
               std::map<std::string, std::string> &actionMap) {
  Records out;
  for (const auto &r : source) {
    if (!event.empty() && r.at("kind").value == "input" &&
        r.at("eventSequence").value == event)
      continue;
    if (!action.empty() && r.object.count("actionSequence") &&
        r.at("actionSequence").value == action)
      continue;
    out.push_back(r);
  }
  std::map<std::string, std::string> refs, acts, updates;
  uint64_t sourceSeq[2] = {};
  for (size_t i = 0; i < out.size(); ++i) {
    auto &r = out[i];
    refs[r.at("recordSequence").value] = std::to_string(i + 1);
    if (r.object.count("actionSequence") && !r.at("actionSequence").null() &&
        !acts.count(r.at("actionSequence").value))
      acts[r.at("actionSequence").value] = std::to_string(acts.size() + 1);
    if (r.at("kind").value == "update" &&
        !updates.count(r.at("updateSequence").value))
      updates[r.at("updateSequence").value] =
          std::to_string(updates.size() + 1);
  }
  for (auto &r : out) {
    r.at("recordSequence") = refs.at(r.at("recordSequence").value);
    if (r.object.count("sourceSequence"))
      r.at("sourceSequence") = std::to_string(
          ++sourceSeq[r.at("source").value == "platform" ? 0 : 1]);
    for (const auto &k : {"mappingSampleSequence", "controlSequence"})
      if (r.object.count(k) && !r.at(k).null()) {
        insist(refs.count(r.at(k).value) != 0, "TraceMissingReference",
               "Removed dependency");
        r.at(k) = refs.at(r.at(k).value);
      }
    if (r.object.count("actionSequence") && !r.at("actionSequence").null())
      r.at("actionSequence") = acts.at(r.at("actionSequence").value);
    if (r.object.count("updateSequence") && !r.at("updateSequence").null()) {
      insist(updates.count(r.at("updateSequence").value) != 0,
             "TraceMissingReference", "Removed update dependency");
      r.at("updateSequence") = updates.at(r.at("updateSequence").value);
    }
  }
  actionMap = acts;
  recount(out);
  return out;
}
void resultSchema(const Json &r) {
  keys(r, "complete diagnosticError decisions judgments score boundaries "
          "pendingEventSequences");
  insist(r.at("complete").yes() && r.at("diagnosticError").null(),
         "TraceIncomplete", "Cannot compare incomplete result");
  for (const auto &k :
       {"decisions", "judgments", "boundaries", "pendingEventSequences"})
    insist(r.at(k).type == Json::Array, "TraceInvalidSchema",
           "Expected result array", {}, k);
  for (const auto &d : r.at("decisions").array) {
    keys(d, "eventSequence disposition reason error");
    insist(uintValue(d.at("eventSequence")) > 0, "TraceInvalidValue",
           "Zero event");
    enumeration(d, "disposition", "accepted rejected cleared");
    enumeration(d, "reason", reasons, true);
    enumeration(d, "error", errors);
  }
  for (const auto &j : r.at("judgments").array) {
    keys(j, "entity type targetTime inputTime error");
    uintValue(j.at("entity"));
    insist(uintValue(j.at("type"), true) <= UINT32_MAX, "TraceInvalidValue",
           "Judgment type out of range");
    for (const auto &k : {"targetTime", "inputTime", "error"})
      intValue(j.at(k));
  }
  keys(r.at("score"), "value perfect miss");
  uintValue(r.at("score").at("value"));
  for (const auto &k : {"perfect", "miss"})
    insist(uintValue(r.at("score").at(k), true) <= UINT32_MAX,
           "TraceInvalidValue", "Score out of range");
  for (const auto &b : r.at("boundaries").array) {
    keys(b, "actionSequence recordSequence activePointers");
    insist(uintValue(b.at("actionSequence")) > 0 &&
               uintValue(b.at("recordSequence")) > 0,
           "TraceInvalidValue", "Zero boundary sequence");
    insist(b.at("activePointers").type == Json::Array, "TraceInvalidSchema",
           "Expected pointers");
    uint64_t prev = 0;
    bool first = true;
    for (const auto &p : b.at("activePointers").array) {
      auto n = uintValue(p, true);
      insist(n <= UINT32_MAX && (first || n > prev), "TraceInvalidValue",
             "Invalid pointers");
      prev = n;
      first = false;
    }
  }
  std::set<std::string> pending;
  for (const auto &id : r.at("pendingEventSequences").array)
    insist(uintValue(id) > 0 && pending.insert(id.value).second,
           "TraceInvalidValue", "Invalid pending");
}
void minimize(const Records &source, const std::string &chart,
              const std::string &audio, const std::string &target,
              const std::filesystem::path &directory) {
  auto initial = runTrace(source, chart, audio);
  Json decision;
  for (const auto &d : initial.result.at("decisions").array)
    if (d.at("eventSequence").value == target &&
        d.at("disposition").value == "rejected")
      decision = d;
  insist(!decision.null(), "TraceTargetNotReproduced",
         "Target is not a reproduced rejection");
  Records current = source;
  current.front().at("provenance").at("origin") = "synthetic";
  Json result = initial.result;
  Json steps = Json::list();
  std::map<std::string, std::string> origins;
  for (const auto &r : source)
    if (r.object.count("actionSequence") && !r.at("actionSequence").null())
      origins[r.at("actionSequence").value] = r.at("actionSequence").value;
  bool changed = true;
  while (changed) {
    changed = false;
    std::vector<std::pair<std::string, std::string>> candidates;
    std::set<std::string> seen;
    for (const auto &r : current)
      if (r.at("kind").value == "input" && r.at("stage").value == "receive" &&
          r.at("eventSequence").value != target)
        candidates.emplace_back(r.at("eventSequence").value, "");
    for (const auto &r : current)
      if (r.object.count("actionSequence") && !r.at("actionSequence").null() &&
          seen.insert(r.at("actionSequence").value).second)
        candidates.emplace_back("", r.at("actionSequence").value);
    for (const auto &candidate : candidates) {
      try {
        std::map<std::string, std::string> mapping;
        auto reduced =
            subset(current, candidate.first, candidate.second, mapping);
        auto run = runTrace(reduced, chart, audio, false);
        bool kept = false;
        for (const auto &d : run.result.at("decisions").array)
          if (d == decision)
            kept = true;
        if (!kept)
          continue;
        runTrace(run.trace, chart, audio);
        Json step = Json::obj(
            {{"removedEvent",
              candidate.first.empty() ? Json() : Json(candidate.first)},
             {"removedSourceAction",
              candidate.second.empty() ? Json()
                                       : Json(origins.at(candidate.second))}});
        std::map<std::string, std::string> next;
        for (const auto &m : mapping)
          next[m.second] = origins.at(m.first);
        origins = std::move(next);
        steps.array.push_back(step);
        current = std::move(run.trace);
        result = std::move(run.result);
        changed = true;
        break;
      } catch (const Problem &p) {
        if (p.exitCode == 3)
          throw;
      } catch (const std::out_of_range &) { /* A removed control owns a
                                               remaining input dependency. */
      }
    }
  }
  Json kept = Json::list(), actionOrigins = Json::list();
  for (const auto &r : current)
    if (r.at("kind").value == "input" && r.at("stage").value == "receive")
      kept.array.push_back(r.at("eventSequence"));
  for (const auto &p : origins)
    actionOrigins.array.push_back(Json::obj(
        {{"actionSequence", p.first}, {"sourceActionSequence", p.second}}));
  Json metadata = Json::obj(
      {{"sourceRunId", source.front().at("runId")},
       {"targetEventSequence", target},
       {"retainedEventSequences", kept},
       {"actions", actionOrigins},
       {"steps", steps},
       {"dependencyClosure",
        "Greedy single-event/action fixed point. Each deletion replayed "
        "against original Session; exact target disposition/reason/error "
        "preserved. Required clock/control/sample references retained; no "
        "claim of globally smallest fixture."}});
  if (std::filesystem::exists(directory))
    throw Problem("TraceIoError", "Output directory exists", {}, {}, 3);
  auto parent = directory.parent_path();
  if (parent.empty())
    parent = ".";
  auto staging =
      parent /
      (directory.filename().string() + ".tmp-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  if (!std::filesystem::create_directory(staging))
    throw Problem("TraceIoError", "Cannot stage minimized fixture", {}, {}, 3);
  try {
    writeNew(staging / "trace.jsonl", jsonl(current));
    writeNew(staging / "result.json", dump(result) + "\n");
    writeNew(staging / "minimization.json", dump(metadata) + "\n");
    writeNew(staging / "chart.json", chart);
    writeNew(staging / "audio.wav", audio);
    std::filesystem::rename(staging, directory);
  } catch (...) {
    std::error_code ec;
    std::filesystem::remove_all(staging, ec);
    throw;
  }
}
} // namespace tracecli
int main(int argc, char **argv) {
  using namespace tracecli;
  std::map<std::string, std::string> args;
  std::string command;
  try {
    insist(argc >= 2, "TraceInvalidSchema",
           "Usage: hrk_trace validate|replay|compare|minimize <options>");
    command = argv[1];
    insist((argc - 2) % 2 == 0, "TraceInvalidSchema", "Options require values");
    for (int i = 2; i < argc; i += 2)
      insist(args.emplace(argv[i], argv[i + 1]).second, "TraceInvalidSchema",
             "Duplicate option");
    std::set<std::string> required;
    if (command == "validate")
      required = {"--trace", "--out"};
    else if (command == "replay")
      required = {"--trace", "--chart", "--audio", "--out"};
    else if (command == "compare")
      required = {"--expected", "--actual", "--out"};
    else if (command == "minimize")
      required = {"--trace", "--chart", "--audio", "--event", "--out-dir"};
    else
      throw Problem("TraceInvalidSchema", "Unknown command");
    insist(args.size() == required.size(), "TraceInvalidSchema",
           "Wrong options");
    for (const auto &key : required)
      insist(args.count(key) && !args.at(key).empty() &&
                 args.at(key).size() <= 4096,
             "TraceInvalidSchema", "Missing/invalid option", {}, key);
    if (command == "compare") {
      auto expected = Parser(readFile(args.at("--expected"))).parse(),
           actual = Parser(readFile(args.at("--actual"))).parse();
      resultSchema(expected);
      resultSchema(actual);
      auto diff = compareResults(expected, actual);
      writeNew(args.at("--out"), dump(diff) + "\n");
      return diff.at("equal").yes() ? 0 : 1;
    }
    auto records = readTrace(args.at("--trace"));
    validateTrace(records);
    if (command == "validate")
      writeNew(
          args.at("--out"),
          dump(Json::obj({{"complete", true},
                          {"diagnosticError", Json()},
                          {"recordCount", std::to_string(records.size())}})) +
              "\n");
    else {
      auto chart = readFile(args.at("--chart")),
           audio = readFile(args.at("--audio"));
      if (command == "replay")
        writeNew(args.at("--out"),
                 dump(runTrace(records, chart, audio).result) + "\n");
      else {
        insist(uintValue(Json(args.at("--event"))) > 0, "TraceInvalidValue",
               "Invalid target event");
        minimize(records, chart, audio, args.at("--event"),
                 args.at("--out-dir"));
      }
    }
    return 0;
  } catch (const Problem &p) {
    auto report = dump(failure(p)) + "\n";
    std::cerr << report;
    if (args.count("--out") && !std::filesystem::exists(args.at("--out")))
      try {
        writeNew(args.at("--out"), report);
      } catch (...) {
        return 3;
      }
    return p.exitCode;
  } catch (const std::exception &e) {
    const auto report =
        dump(failure(Problem("TraceIoError", e.what(), {}, {}, 3))) + "\n";
    std::cerr << report;
    try {
      if (args.count("--out") && !std::filesystem::exists(args.at("--out")))
        writeNew(args.at("--out"), report);
    } catch (...) {
    }
    return 3;
  }
}
