/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <v8/src/builtins/builtins.cc>

#if BUILDFLAG(ENABLE_BRAVE_PAGE_GRAPH_WEBAPI_PROBES)
#include "src/builtins/builtins-inl.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins-utils.h"
#endif  // BUILDFLAG(ENABLE_BRAVE_PAGE_GRAPH_WEBAPI_PROBES)

namespace v8::internal {

#if BUILDFLAG(ENABLE_BRAVE_PAGE_GRAPH_WEBAPI_PROBES)
static std::string ToPageGraphArg(Isolate* isolate, Handle<Object> object) {
#ifdef OBJECT_PRINT  // Enabled with v8_enable_object_print=true gn arg.
  std::ostringstream stream;
  Print(*object, stream);
  return stream.str();
#else   // OBJECT_PRINT
  if (object.is_null()) {
    return {};
  }
  MaybeDirectHandle<String> maybe_string =
      Object::NoSideEffectsToMaybeString(isolate, object);
  DirectHandle<String> string_handle;
  if (!maybe_string.ToHandle(&string_handle)) {
    return {};
  }
  if (auto c_string = string_handle->ToCString()) {
    return std::string(c_string.get());
  }
  return {};
#endif  // OBJECT_PRINT
}

void ReportBuiltinCallAndResponse(Isolate* isolate,
                                  const char* builtin_name,
                                  const BuiltinArguments& builtin_args,
                                  const Tagged<Object>& builtin_result) {
  HandleScope scope(isolate);

  // Root the result BEFORE serializing the arguments below. ToPageGraphArg ->
  // Object::NoSideEffectsToMaybeString allocates (it builds strings) and can
  // trigger a GC; `builtin_result` arrives as a raw, unrooted Tagged<Object>, so
  // a GC during arg serialization would move the result object and leave the
  // raw reference dangling — later dereferencing it reads a moved/garbage map
  // pointer and faults (SIGBUS/BUS_ADRALN on builtins with args + a movable
  // result, e.g. JSON.stringify). Binding it to a Handle here lets the GC update
  // it. (Arguments/receiver come from BuiltinArguments and are already rooted in
  // the frame, so they don't need this.)
  const bool has_result =
      builtin_result.ptr() && !IsUndefined(builtin_result);
  Handle<Object> result_handle =
      has_result ? Handle<Object>(builtin_result, isolate) : Handle<Object>();

  std::vector<std::string> args;
  // Start from 1 to skip receiver arg.
  for (int arg_idx = 1; arg_idx < builtin_args.length(); ++arg_idx) {
    args.push_back(ToPageGraphArg(isolate, builtin_args.at(arg_idx)));
  }

  std::optional<std::string> result;
  if (has_result) {
    result = ToPageGraphArg(isolate, result_handle);
  }

  v8::Isolate* execution_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  v8::Local<v8::Context> context = execution_isolate->GetCurrentContext();

  Handle<Object> receiver = builtin_args.receiver();
  if (IsJSReceiver(*receiver)) {
    v8::Local<v8::Value> receiver_value = Utils::ToLocal(receiver);
    v8::Local<v8::Object> receiver_object =
        v8::Local<v8::Object>::Cast(receiver_value);
    v8::MaybeLocal<v8::Context> maybe_receiver_creation_context =
        receiver_object->GetCreationContext();
    if (!maybe_receiver_creation_context.IsEmpty()) {
      context = maybe_receiver_creation_context.ToLocalChecked();
    }
  }

  isolate->page_graph_delegate()->OnBuiltinCall(context, builtin_name, args,
                                                result ? &*result : nullptr);
}
#endif  // BUILDFLAG(ENABLE_BRAVE_PAGE_GRAPH_WEBAPI_PROBES)

}  // namespace v8::internal
