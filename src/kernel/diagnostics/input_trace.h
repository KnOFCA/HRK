#pragma once
#include "interface/platform/platform.h"
#include <memory>
#include <ostream>
#include <type_traits>

namespace hrk {
enum class CaptureError { Ok, CaptureInvalidState, CaptureAllocationFailed, CaptureFull,
  CaptureCounterOverflow, CaptureConcurrencyUnsupported, CaptureBusy, CaptureExportFailed,
  TraceIncomplete, TraceMissingReference };
const char* captureErrorName(CaptureError);
enum class TraceSource { Platform, Session };
enum class TraceKind { Input, Clock, Control, Update };
enum class TraceStage { Receive, PlatformEnqueue, PlatformPoll, SessionEnqueue, Map, Pending, Terminal };
enum class TraceDisposition { Observed, Queued, Pending, Accepted, Rejected, Cleared };
enum class TraceReason { None, BeforeEpoch, MappedOutOfRange, BeforeWatermark, PlatformQueueFull,
  SessionQueueFull, InvalidInput, InvalidState, ReplayFull, LifecycleClear, InactiveDrain, Count };
enum class TraceOperation { Load, Start, Pause, Resume, Seek, Stop, Background, Foreground,
  SurfaceCreate, SurfaceResize, SurfaceDestroy, Render, Status, Update, Measure, Request120Hz, UnknownCommand };
enum class TraceQuery { Position, ClockSample, Anchor };
struct TraceContext {
  uint64_t event=0, batch=0, epoch=0, generation=0;
  TimeNs received=0;
  uint32_t point=0;
};
struct InputEnvelope { RawInputEvent raw; TraceContext trace; };
struct TraceInput {
  InputEnvelope event;
  TimeNs consumed=0, mapped=0, before=0, after=0, now=0;
  uint64_t sample=0;
  uint32_t valid=0;
  TraceStage stage=TraceStage::Receive;
  TraceDisposition disposition=TraceDisposition::Observed;
  TraceReason reason=TraceReason::None;
  Error error=Error::Ok;
};
enum TraceValue : uint32_t { ConsumeValue=1, MappedValue=2, WatermarkValue=4, SongValue=8 };
struct TraceClock {
  TimeNs host, song, position;
  TraceQuery query;
  TraceOperation caller;
  uint32_t index;
  bool success;
};
struct TraceBoundary {
  TimeNs host, target, before, after, now;
  std::array<uint32_t,32> pointers;
  uint32_t pointerCount, valid;
  TraceOperation operation;
  Error result;
  bool end;
};
struct TraceRecord {
  uint64_t epoch=0, action=0, control=0, update=0;
  TraceKind kind=TraceKind::Input;
  union Payload { TraceInput input; TraceClock clock; TraceBoundary boundary; Payload():input{}{} } data;
};
static_assert(std::is_trivially_copyable_v<TraceRecord>);
struct TraceSlot { TraceRecord record; std::atomic<bool> ready{false}; };
static_assert(sizeof(TraceSlot)<=256, "S1 record budget");
struct TraceHeader {
  std::string runId, sourceRevision, specRevision, policyVersion="v0.1.0-observe", buildMode;
  std::string deviceModel, osVersion, chartSha256, audioSha256, origin;
  double actualRefreshHz=0; // zero means unknown, exported as null
  TimeNs inputDeliveryGrace=0;
};
struct TraceSummary {
  uint64_t ingress=0, accepted=0, rejected=0, cleared=0, pending=0;
  uint64_t platformDropped=0, sessionDropped=0, platformQueueDropped=0, sessionQueueDropped=0;
  bool complete=false;
};

// The object outlives its input backend. Only the control owner may begin/end/export.
// A platform Writer covers the whole callback, including queue submission.
class InputTrace {
  friend struct InputTraceTestAccess;
public:
  static constexpr size_t capacity=65536;
  static constexpr size_t memoryBudget=34603008;
  static constexpr uint64_t sessionHandle=uint64_t{1}<<63;
  class Writer {
    InputTrace* trace_=nullptr;
  public:
    explicit Writer(InputTrace*);
    explicit Writer(InputTrace& t):Writer(&t){}
    ~Writer();
    Writer(const Writer&)=delete; Writer& operator=(const Writer&)=delete;
    explicit operator bool() const {return trace_!=nullptr;}
  };
private:
  std::unique_ptr<TraceSlot[]> platform_, session_;
  // One byte per possible retained ingress for offline reconciliation only.
  std::unique_ptr<uint8_t[]> eventStates_;
  TraceHeader header_;
  std::atomic<bool> open_{false};
  std::atomic<uint64_t> writers_{0}, epoch_{0}, events_{0}, batches_{0};
  std::atomic<uint64_t> used_[2]{}, dropped_[2]{}, counts_[5][3][static_cast<size_t>(TraceReason::Count)]{};
  std::atomic<uint32_t> issues_{0};
  uint64_t generation_=0, actionCounter_=0, updateCounter_=0, action_=0, control_=0, update_=0;
  uint32_t queryIndex_=0;
  TraceOperation caller_=TraceOperation::Status;
  bool frozen_=false;
  TraceSummary summary_{};
  uint64_t increment(std::atomic<uint64_t>&);
  uint64_t next(uint64_t&);
  uint64_t append(TraceSource,const TraceRecord&);
  void reconcile();
  void writeJson(std::ostream&) const;
public:
  ~InputTrace();
  CaptureError beginCapture(const TraceHeader&,bool playingOrPaused);
  CaptureError endCapture(bool playing);
  CaptureError exportCapture(const std::string& path) const;
  bool active() const {return open_.load(std::memory_order_acquire);}
  bool frozen() const {return frozen_;}
  bool belongs(const TraceContext& c) const {return c.event && c.generation==generation_;}
  uint64_t epoch() const {return epoch_.load(std::memory_order_acquire);}
  size_t allocatedBytes() const;
  const TraceSummary& summary() const {return summary_;}
  uint64_t recordCount(TraceSource) const;
  const TraceRecord& record(TraceSource,size_t) const;
  uint64_t sequence(uint64_t handle) const;
  uint64_t batch() {return increment(batches_);}
  InputEnvelope receive(const RawInputEvent&,TimeNs received,uint64_t batch=0,uint32_t point=0,TraceSource source=TraceSource::Platform);
  void input(TraceSource,const TraceInput&);
  void platformEnqueue(const InputEnvelope&,bool queued);
  void platformPoll(const InputEnvelope&);
  void cleared(const InputEnvelope&,TraceReason=TraceReason::LifecycleClear);
  void issue(CaptureError);
  uint64_t clock(TraceQuery,TimeNs host,TimeNs song,TimeNs position,bool success);
  uint64_t anchor(TimeNs host,TimeNs song);
  struct Scope {
    InputTrace* trace;
    uint64_t oldAction, oldControl, oldUpdate;
    TraceOperation oldCaller;
    TraceRecord begin;
    Scope(InputTrace*,TraceOperation,TimeNs host,TimeNs target=0,bool update=false);
    ~Scope();
    Scope(const Scope&)=delete; Scope& operator=(const Scope&)=delete;
    void finish(Error,const std::array<uint32_t,32>&,uint32_t,TimeNs before=0,TimeNs after=0,TimeNs now=0,bool hasTimes=false);
  };
  // Poll transfer is an action without a control boundary or audio query.
  struct Transfer {
    InputTrace* trace; uint64_t old;
    explicit Transfer(InputTrace*);
    ~Transfer();
  };
};
static_assert(2*InputTrace::capacity*sizeof(TraceSlot)<=32*1024*1024);
static_assert(sizeof(InputTrace)+InputTrace::capacity+4098*sizeof(TraceContext)<1024*1024);

// Shared by the native backend and host tests. The original SPSC contract remains.
class TracedPlatformQueue {
  FixedQueue<InputEnvelope,2048> queue_;
  InputTrace* trace_=nullptr;
  std::atomic<uint64_t> dropped_{0};
public:
  void setTrace(InputTrace* t){trace_=t;}
  bool submit(const RawInputEvent& raw,TimeNs received,uint64_t batch,uint32_t point,bool capturing){
    auto e=capturing?trace_->receive(raw,received,batch,point):InputEnvelope{raw,{}};
    const bool ok=queue_.push(e);if(!ok)++dropped_;
    if(capturing)trace_->platformEnqueue(e,ok);
    return ok;
  }
  void rejectInvalid(const RawInputEvent& raw,TimeNs received,uint64_t batch,uint32_t point,bool capturing){
    if(!capturing)return;
    TraceInput i;i.event=trace_->receive(raw,received,batch,point);i.stage=TraceStage::Terminal;
    i.disposition=TraceDisposition::Rejected;i.reason=TraceReason::InvalidInput;i.error=Error::InvalidArgument;
    trace_->input(TraceSource::Platform,i);
  }
  bool poll(InputEnvelope& e){return queue_.pop(e);}
  void clear(){if(trace_ && trace_->active())queue_.clearObserved([&](const InputEnvelope& e){trace_->cleared(e);});else queue_.clear();}
  uint64_t dropped() const{return dropped_.load();}
};

// Observes the public calls actually made by SongClock/Session, without querying again.
class TraceAudioBackend : public IAudioBackend {
  IAudioBackend& backend_; InputTrace* trace_=nullptr; mutable uint64_t lastSample_=0;
public:
  explicit TraceAudioBackend(IAudioBackend& b):backend_(b){}
  void trace(InputTrace* t){trace_=t;}
  uint64_t lastSample() const{return lastSample_;}
  Error load(const Pcm& p) override{return backend_.load(p);}
  Error start() override{return backend_.start();}
  Error pause() override{return backend_.pause();}
  Error resume() override{return backend_.resume();}
  Error seek(TimeNs t) override{return backend_.seek(t);}
  void stop() override{backend_.stop();}
  TimeNs position() const override;
  bool clockSample(TimeNs&,TimeNs&) const override;
  TimeNs duration() const override{return backend_.duration();}
  TimeNs inputDeliveryGrace() const override{return backend_.inputDeliveryGrace();}
};
}
