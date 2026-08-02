/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/renderer/core/brave_page_graph/stack_trace_capture.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/environment.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/platform/bindings/thread_debugger.h"
#include "third_party/inspector_protocol/crdtp/json.h"
#include "third_party/inspector_protocol/crdtp/span.h"
#include "v8/include/v8-inspector-protocol.h"
#include "v8/include/v8-inspector.h"
#include "v8/include/v8-isolate.h"

namespace brave_page_graph {

namespace {
// Depth of the async `parent` chain to walk (setTimeout/Promise/fetch
// callbacks, etc.). Cookie writes frequently happen several async hops away
// from the originating user code, so this default is generous. Kept in sync
// with the maxDepth passed to Debugger.setAsyncCallStackDepth below (both come
// from AsyncStackDepth()).
constexpr int kDefaultMaxAsyncDepth = 128;

// Tunable stack-capture cost, read once from the environment so it can be
// dialed down (without a rebuild) on heavy sites where the extra per-call
// native-stack overhead of full async recording + per-edge full-stack walks
// tips a deep JS recursion over the native stack guard page (SIGBUS) — see the
// "harden JS-stack-overflow" work. Defaults reproduce the original behavior.

// PAGEGRAPH_ASYNC_STACK_DEPTH: async parent-chain depth. 0 disables async
// call-stack recording entirely (no inspector session). Default 128.
int AsyncStackDepth() {
  static const int depth = [] {
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    const std::optional<std::string> v =
        env->GetVar("PAGEGRAPH_ASYNC_STACK_DEPTH");
    int parsed = 0;
    if (v && !v->empty() && base::StringToInt(*v, &parsed) && parsed >= 0) {
      return parsed;
    }
    return kDefaultMaxAsyncDepth;
  }();
  return depth;
}

// PAGEGRAPH_EDGE_STACK: per-edge capture mode. "off" = capture nothing (edges
// get no stack trace); "full" = the original unbounded walk (fullStack=true);
// anything else / unset = "top" (fullStack=false), which respects the
// inspector's default capture limit.
//
// "top" is the DEFAULT because "full" costs a renderer crash and buys no
// information. Measured on cnn.com, same page, both modes:
//
//     mode   stacks   avg frames   max depth   with async parent
//     full   41,334      6.4          88             78%
//     top    44,984      6.3          88             81%
//
// The depth histograms match to within a couple of percent per bucket, so the
// unbounded walk surfaces nothing the bounded one misses. The only frames past
// the default limit belong to pathological deep recursions, and walking those
// on every edge is what tips the native stack over its guard page -- the
// recording-time crash seen on nytimes.com and pgatour.com. Async parent chains
// are unaffected either way: they are walked separately below via
// buildInspectorObject(AsyncStackDepth()).
enum class EdgeStackMode { kOff, kTop, kFull };
EdgeStackMode EdgeStackCaptureMode() {
  static const EdgeStackMode mode = [] {
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    const std::optional<std::string> v = env->GetVar("PAGEGRAPH_EDGE_STACK");
    if (v && *v == "off") {
      return EdgeStackMode::kOff;
    }
    if (v && *v == "full") {
      return EdgeStackMode::kFull;
    }
    return EdgeStackMode::kTop;
  }();
  return mode;
}

// Discards all inspector protocol traffic: we only connect a session to enable
// async stack collection, never to read responses.
class NoopInspectorChannel : public v8_inspector::V8Inspector::Channel {
 public:
  NoopInspectorChannel() = default;
  ~NoopInspectorChannel() override = default;
  void sendResponse(int,
                    std::unique_ptr<v8_inspector::StringBuffer>) override {}
  void sendNotification(std::unique_ptr<v8_inspector::StringBuffer>) override {}
  void flushProtocolNotifications() override {}
};

// Keeps the async-stack-enabling session alive for the isolate's lifetime. The
// channel must outlive the session, so declaration order matters here.
struct AsyncStackCaptureSession {
  NoopInspectorChannel channel;
  std::unique_ptr<v8_inspector::V8InspectorSession> session;
};

v8_inspector::StringView ToStringView(const char* str) {
  return v8_inspector::StringView(reinterpret_cast<const uint8_t*>(str),
                                  std::strlen(str));
}
}  // namespace

std::string CaptureStackTraceJson() {
  const EdgeStackMode mode = EdgeStackCaptureMode();
  if (mode == EdgeStackMode::kOff) {
    return std::string();
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate || !isolate->InContext()) {
    return std::string();
  }

  blink::ThreadDebugger* thread_debugger =
      blink::ThreadDebugger::From(isolate);
  if (!thread_debugger) {
    return std::string();
  }
  v8_inspector::V8Inspector* inspector = thread_debugger->GetV8Inspector();
  if (!inspector) {
    return std::string();
  }

  // fullStack=false captures only the shallow synchronous top frames, avoiding
  // an O(stack-depth) walk on every edge — the headroom/cost win on deep
  // recursions. fullStack=true is the original full walk.
  const bool full_stack = mode == EdgeStackMode::kFull;
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      inspector->captureStackTrace(full_stack);
  if (!stack_trace || stack_trace->isEmpty()) {
    return std::string();
  }

  std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
      inspector_object = stack_trace->buildInspectorObject(AsyncStackDepth());
  if (!inspector_object) {
    return std::string();
  }

  std::vector<uint8_t> cbor;
  inspector_object->AppendSerialized(&cbor);

  std::string json;
  crdtp::Status status =
      crdtp::json::ConvertCBORToJSON(crdtp::SpanFrom(cbor), &json);
  if (!status.ok()) {
    return std::string();
  }
  return json;
}

void EnsureAsyncStackCaptureEnabled(int context_group_id) {
  static bool enabled = false;
  if (enabled) {
    return;
  }

  // Depth 0 disables async recording entirely: we skip connecting the inspector
  // session, so the isolate pays none of the per-call async-bookkeeping cost.
  // Synchronous per-edge capture (if not "off") still works without it.
  const int async_depth = AsyncStackDepth();
  if (async_depth == 0) {
    enabled = true;
    return;
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate) {
    return;
  }
  blink::MainThreadDebugger* debugger =
      blink::MainThreadDebugger::Instance(isolate);
  if (!debugger) {
    return;
  }
  v8_inspector::V8Inspector* inspector = debugger->GetV8Inspector();
  if (!inspector) {
    return;
  }

  static base::NoDestructor<AsyncStackCaptureSession> capture_session;
  capture_session->session = inspector->connect(
      context_group_id, &capture_session->channel, v8_inspector::StringView(),
      v8_inspector::V8Inspector::kFullyTrusted);
  if (!capture_session->session) {
    return;
  }
  // Enabling the Runtime agent lets Debugger.setAsyncCallStackDepth take effect
  // without pausing execution; setAsyncCallStackDepth then turns on isolate-wide
  // async parent-chain recording.
  capture_session->session->dispatchProtocolMessage(
      ToStringView("{\"id\":1,\"method\":\"Runtime.enable\"}"));
  const std::string set_depth_msg =
      "{\"id\":2,\"method\":\"Debugger.setAsyncCallStackDepth\","
      "\"params\":{\"maxDepth\":" +
      base::NumberToString(async_depth) + "}}";
  capture_session->session->dispatchProtocolMessage(
      ToStringView(set_depth_msg.c_str()));
  enabled = true;
}

}  // namespace brave_page_graph
