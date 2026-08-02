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
                                  Tagged<Object>* builtin_result) {
  HandleScope scope(isolate);

  // Root the result BEFORE serializing the arguments below, and write the
  // rooted value back at the end. Everything under this function allocates
  // (ToPageGraphArg -> Object::NoSideEffectsToMaybeString builds strings, and
  // the PageGraph delegate allocates further), so a GC can run here.
  // `*builtin_result` is the raw, unrooted Tagged<Object> that the BUILTIN
  // macro is about to return to generated code; if a GC moves that object,
  // the macro hands a stale address back to JS. The JS value then looks like
  // a heap object with a garbage map, and the next property load on it faults
  // (SIGBUS/BUS_ADRALN) or trips V8's "null prototype chain root" check --
  // typically far away from here, in unrelated script.
  //
  // Binding it to a Handle lets the GC update it, but the update lands in the
  // handle, not in the caller's raw variable, so the new address must be
  // copied back through the pointer before we return.
  //
  // (Arguments and the receiver come from BuiltinArguments and are already
  // rooted in the frame, so they don't need this.)
  const bool has_result =
      builtin_result->ptr() && !IsUndefined(*builtin_result);
  Handle<Object> result_handle =
      has_result ? Handle<Object>(*builtin_result, isolate) : Handle<Object>();

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

  // Hand the possibly-relocated address back to the BUILTIN macro. Must happen
  // before `scope` is destroyed.
  if (has_result) {
    *builtin_result = *result_handle;
  }
}
#endif  // BUILDFLAG(ENABLE_BRAVE_PAGE_GRAPH_WEBAPI_PROBES)

}  // namespace v8::internal
