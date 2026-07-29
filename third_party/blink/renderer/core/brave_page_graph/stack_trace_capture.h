/* Copyright (c) 2026 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_THIRD_PARTY_BLINK_RENDERER_CORE_BRAVE_PAGE_GRAPH_STACK_TRACE_CAPTURE_H_
#define BRAVE_THIRD_PARTY_BLINK_RENDERER_CORE_BRAVE_PAGE_GRAPH_STACK_TRACE_CAPTURE_H_

#include <string>

#include "third_party/blink/renderer/core/core_export.h"

namespace brave_page_graph {

// Captures the JS call stack of the currently-executing script on the current
// isolate and returns it as a JSON-serialized DevTools `Runtime.StackTrace`
// object (synchronous frames plus the async `parent` chain). Each frame carries
// `functionName`, `scriptId`, `url`, `lineNumber`, and `columnNumber`; the
// `scriptId` matches the "script id" attribute PageGraph writes on script nodes,
// so frames are directly joinable to the graph.
//
// Returns an empty string when there is no script on the stack (edges created by
// the HTML parser, network callbacks, or engine bookkeeping), when no isolate is
// in context, or when the inspector is unavailable. Callers should only attach
// the result when it is non-empty.
CORE_EXPORT std::string CaptureStackTraceJson();

// Turns on isolate-wide async call-stack collection so that CaptureStackTraceJson
// can recover the async `parent` chain (callbacks scheduled via setTimeout,
// promises, fetch, etc.). Without this, only synchronous frames are available.
// Idempotent and cheap after the first call; connects a single no-op inspector
// session kept alive for the lifetime of the (main-thread) isolate.
// `context_group_id` is the inspector context group of any frame on the isolate.
CORE_EXPORT void EnsureAsyncStackCaptureEnabled(int context_group_id);

}  // namespace brave_page_graph

#endif  // BRAVE_THIRD_PARTY_BLINK_RENDERER_CORE_BRAVE_PAGE_GRAPH_STACK_TRACE_CAPTURE_H_
