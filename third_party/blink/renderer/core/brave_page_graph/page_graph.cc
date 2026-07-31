/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/renderer/core/brave_page_graph/page_graph.h"

#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <signal.h>

#include <algorithm>
#include <climits>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/environment.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "brave/components/brave_page_graph/common/features.h"
#include "brave/components/brave_shields/core/common/brave_shield_constants.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/blink_probe_types.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/attribute/edge_attribute_delete.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/attribute/edge_attribute_set.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/binding/edge_binding.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/binding/edge_binding_event.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/edge_cross_dom.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/edge_document.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/edge_filter.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/edge_resource_block.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/edge_shield.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/edge_structure.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/edge_text_change.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/event_listener/edge_event_listener_add.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/event_listener/edge_event_listener_remove.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/execute/edge_execute.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/execute/edge_execute_attr.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/js/edge_js_call.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/js/edge_js_result.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/node/edge_node_create.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/node/edge_node_insert.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/node/edge_node_remove.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/storage/edge_storage_bucket.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/storage/edge_storage_clear.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/storage/edge_storage_delete.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/storage/edge_storage_read_call.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/storage/edge_storage_read_result.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/storage/edge_storage_set.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/actor/node_actor.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/actor/node_parser.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/actor/node_script.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/actor/node_script_local.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/actor/node_script_remote.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/actor/node_unknown.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/binding/node_binding.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/binding/node_binding_event.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/filter/node_ad_filter.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/filter/node_fingerprinting_filter.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/filter/node_tracker_filter.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/html/node_dom_root.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/html/node_frame_owner.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/html/node_html.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/html/node_html_element.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/html/node_html_text.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/js/node_js.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/js/node_js_builtin.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/js/node_js_webapi.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/node_extensions.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/node_resource.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/shield/node_shield.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/shield/node_shields.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/storage/node_storage_cookiejar.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/storage/node_storage_localstorage.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/storage/node_storage_root.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/storage/node_storage_sessionstorage.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graphml.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/libxml_utils.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/requests/request_tracker.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/requests/tracked_request.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/scripts/script_tracker.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/stack_trace_capture.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/types.h"
#include "brave/v8/include/v8-isolate-page-graph-utils.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-shared.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_creation_params.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/script/classic_pending_script.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/script/module_pending_script.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/platform/bindings/string_resource.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

using brave_page_graph::DocumentRequest;
using brave_page_graph::EdgeAttributeDelete;
using brave_page_graph::EdgeAttributeSet;
using brave_page_graph::EdgeBinding;
using brave_page_graph::EdgeBindingEvent;
using brave_page_graph::EdgeCrossDOM;
using brave_page_graph::EdgeEventListenerAdd;
using brave_page_graph::EdgeEventListenerRemove;
using brave_page_graph::EdgeExecute;
using brave_page_graph::EdgeExecuteAttr;
using brave_page_graph::EdgeFilter;
using brave_page_graph::EdgeJSCall;
using brave_page_graph::EdgeJSResult;
using brave_page_graph::EdgeNodeCreate;
using brave_page_graph::EdgeNodeInsert;
using brave_page_graph::EdgeNodeRemove;
using brave_page_graph::EdgeResourceBlock;
using brave_page_graph::EdgeShield;
using brave_page_graph::EdgeStorageBucket;
using brave_page_graph::EdgeStorageClear;
using brave_page_graph::EdgeStorageDelete;
using brave_page_graph::EdgeStorageReadCall;
using brave_page_graph::EdgeStorageReadResult;
using brave_page_graph::EdgeStorageSet;
using brave_page_graph::EdgeStructure;
using brave_page_graph::EdgeTextChange;
using brave_page_graph::FrameId;
using brave_page_graph::GraphItem;
using brave_page_graph::GraphItemId;
using brave_page_graph::ItemName;
using brave_page_graph::NodeActor;
using brave_page_graph::NodeAdFilter;
using brave_page_graph::NodeBinding;
using brave_page_graph::NodeBindingEvent;
using brave_page_graph::NodeDOMRoot;
using brave_page_graph::NodeFingerprintingFilter;
using brave_page_graph::NodeFrameOwner;
using brave_page_graph::NodeHTML;
using brave_page_graph::NodeHTMLElement;
using brave_page_graph::NodeHTMLText;
using brave_page_graph::NodeJSBuiltin;
using brave_page_graph::NodeJSWebAPI;
using brave_page_graph::NodeResource;
using brave_page_graph::NodeScript;
using brave_page_graph::NodeScriptLocal;
using brave_page_graph::NodeScriptRemote;
using brave_page_graph::NodeStorage;
using brave_page_graph::NodeTrackerFilter;
using brave_page_graph::NodeUnknown;
using brave_page_graph::ScriptId;
using brave_page_graph::TrackedRequest;
using brave_page_graph::XmlUtf8String;

namespace blink {

namespace {

constexpr char kPageGraphVersion[] = "0.7.7";
constexpr char kPageGraphUrl[] =
    "https://github.com/brave/brave-browser/wiki/PageGraph";

FrameId GetFrameId(blink::LocalFrame& frame) {
  return blink::DOMNodeIds::IdForNode(frame.GetDocument());
}

FrameId GetFrameId(blink::DocumentLoader* loader) {
  LocalFrame* frame = loader->GetFrame();
  CHECK(frame);
  blink::Document* doc = frame->GetDocument();
  CHECK(doc);
  return blink::DOMNodeIds::IdForNode(doc);
}

FrameId GetFrameId(blink::ExecutionContext* execution_context) {
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(execution_context);
  CHECK(window);
  return blink::DOMNodeIds::IdForNode(window->document());
}

FrameId GetFrameId(blink::Node* node) {
  blink::ExecutionContext* execution_context = node->GetExecutionContext();
  CHECK(execution_context);
  return GetFrameId(execution_context);
}

PageGraph* GetPageGraphFromIsolate(v8::Isolate* isolate) {
  blink::LocalDOMWindow* window = blink::CurrentDOMWindow(isolate);
  if (!window) {
    return nullptr;
  }
  blink::LocalFrame* frame = window->GetFrame();
  if (!frame) {
    frame = window->GetDisconnectedFrame();
  }
  if (!frame) {
    return nullptr;
  }

  if (auto* top_local_frame =
          blink::DynamicTo<blink::LocalFrame>(&frame->Tree().Top())) {
    return blink::PageGraph::From(*top_local_frame);
  } else {
    return blink::PageGraph::From(*frame);
  }
}

class V8PageGraphDelegate : public v8::page_graph::PageGraphDelegate {
 public:
  void OnEvalScriptCompiled(v8::Isolate* isolate,
                            const int script_id,
                            v8::Local<v8::String> source) override {
    if (auto* page_graph = GetPageGraphFromIsolate(isolate)) {
      page_graph->RegisterV8ScriptCompilationFromEval(isolate, script_id,
                                                      source);
    }
  }

#if BUILDFLAG(ENABLE_BRAVE_PAGE_GRAPH_WEBAPI_PROBES)
  void OnBuiltinCall(v8::Local<v8::Context> receiver_context,
                     const char* builtin_name,
                     const std::vector<std::string>& args,
                     const std::string* result) override {
    blink::ExecutionContext* receiver_execution_context =
        blink::ToExecutionContext(receiver_context);
    v8::Isolate* isolate = v8::Isolate::GetCurrent();

    if (auto* page_graph = GetPageGraphFromIsolate(isolate)) {
      blink::PageGraphValues arguments;
      arguments.reserve(args.size());
      auto to_safe_base_value = [](const std::string& arg) {
        if (!base::IsStringUTF8AllowingNoncharacters(arg)) {
          return base::Value(
              base::StrCat({"__pg_base64_encoded__", base::Base64Encode(arg)}));
        }
        return base::Value(arg);
      };
      std::ranges::for_each(args,
                            [&arguments, &to_safe_base_value](const auto& arg) {
                              arguments.Append(to_safe_base_value(arg));
                            });
      std::optional<blink::PageGraphValue> result_value;
      if (result) {
        result_value = to_safe_base_value(*result);
      }

      page_graph->RegisterV8JSBuiltinCall(
          receiver_execution_context, builtin_name, arguments, result_value);
    }
  }
#endif  // BUILDFLAG(ENABLE_BRAVE_PAGE_GRAPH_WEBAPI_PROBES)
};

static v8::Local<v8::Function> GetInnermostFunction(
    v8::Local<v8::Function> function) {
  while (true) {
    v8::Local<v8::Value> bound_function = function->GetBoundFunction();
    if (bound_function->IsFunction()) {
      function = bound_function.As<v8::Function>();
    } else {
      break;
    }
  }

  return function;
}

static int GetListenerScriptId(blink::EventTarget* event_target,
                               blink::EventListener* listener) {
  blink::JSBasedEventListener* const js_listener =
      DynamicTo<blink::JSBasedEventListener>(listener);
  if (!js_listener) {
    return 0;
  }

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> maybe_listener_function =
      js_listener->GetEffectiveFunction(*event_target);
  if (!maybe_listener_function->IsFunction()) {
    return 0;
  }

  v8::Local<v8::Function> listener_function =
      GetInnermostFunction(maybe_listener_function.As<v8::Function>());
  return listener_function->ScriptId();
}

static void AssignSecurityOriginToNodeDOMRoot(
    blink::Document* document,
    brave_page_graph::NodeDOMRoot* node_domroot) {
  if (document->GetExecutionContext()) {
    const auto* security_origin =
        document->GetExecutionContext()->GetSecurityOrigin();
    if (security_origin) {
      node_domroot->SetSecurityOrigin(security_origin->ToString());
    }
  }
}

}  // namespace

namespace {

// --- Streamed-GraphML serialization helpers, shared by ToGraphML's streamed
// --- branch and the Tier C record-time event log so both emit byte-identical
// --- GraphML. ---

// xmlOutputWriteCallback -> base::File. Writes all bytes or fails; libxml aborts
// the save on a -1 return.
int WriteFileCallback(void* context, const char* buffer, int len) {
  auto* out_file = static_cast<base::File*>(context);
  const bool ok = out_file->WriteAtCurrentPosAndCheck(base::as_bytes(
      UNSAFE_BUFFERS(base::span(buffer, base::checked_cast<size_t>(len)))));
  return ok ? len : -1;
}

// The <graphml>/<graph> container tags are fixed literals so we can leave
// <graph> open and stream its children one at a time. Every element with
// dynamic/escaped content (<desc>, <key>, <node>, <edge>) still goes through
// libxml (xmlSaveTree), so escaping/encoding is byte-identical to the inline
// path. These mirror exactly what the xmlNewNs / xmlSetProp calls emit.
constexpr std::string_view kStreamHeaderLiteral =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<graphml xmlns=\"http://graphml.graphdrawing.org/xmlns\""
    " xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
    " xsi:schemaLocation=\"http://graphml.graphdrawing.org/xmlns"
    " http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd\">\n";
constexpr std::string_view kStreamGraphOpenLiteral =
    "<graph id=\"G\" edgedefault=\"directed\">\n";
constexpr std::string_view kStreamFooterLiteral = "</graph>\n</graphml>\n";

// Flush any buffered libxml output first, then write a fixed literal straight to
// the file, keeping the two write paths in strict order. Returns false on write
// failure.
bool WriteStreamLiteral(xmlSaveCtxtPtr save_ctxt,
                        base::File& file,
                        std::string_view literal) {
  xmlSaveFlush(save_ctxt);
  return file.WriteAtCurrentPosAndCheck(base::as_bytes(
      UNSAFE_BUFFERS(base::span(literal.data(), literal.size()))));
}

// Builds the shared GraphML header DOM: the <graphml> root (+ namespaces), the
// <desc> block, and every <key> definition, plus an empty <graph id="G"> child
// returned via out_graph_node. Caller owns the doc (xmlFreeDoc). Used by both
// ToGraphML paths and the event log so the <key> set never drifts.
xmlDocPtr BuildStreamedHeaderDoc(brave_page_graph::FrameId frame_id,
                                 bool is_root,
                                 const String& source_url,
                                 int64_t end_ms,
                                 xmlNodePtr* out_graph_node) {
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root = xmlNewNode(nullptr, BAD_CAST "graphml");
  xmlDocSetRootElement(doc, root);
  xmlNewNs(root, BAD_CAST "http://graphml.graphdrawing.org/xmlns", nullptr);
  xmlNsPtr xsi_ns = xmlNewNs(
      root, BAD_CAST "http://www.w3.org/2001/XMLSchema-instance",
      BAD_CAST "xsi");
  xmlNewNsProp(root, xsi_ns, BAD_CAST "schemaLocation",
               BAD_CAST
               "http://graphml.graphdrawing.org/xmlns "
               "http://graphml.graphdrawing.org/xmlns/1.0/graphml.xsd");

  xmlNodePtr desc = xmlNewChild(root, nullptr, BAD_CAST "desc", nullptr);
  xmlNewTextChild(desc, nullptr, BAD_CAST "version", BAD_CAST kPageGraphVersion);
  xmlNewTextChild(desc, nullptr, BAD_CAST "about", BAD_CAST kPageGraphUrl);
  xmlNewTextChild(desc, nullptr, BAD_CAST "is_root",
                  BAD_CAST(is_root ? "true" : "false"));
  xmlNewTextChild(desc, nullptr, BAD_CAST "frame_id",
                  XmlUtf8String(frame_id).get());
  if (is_root) {
    xmlNewTextChild(desc, NULL, BAD_CAST "url",
                    XmlUtf8String(source_url).get());
  }
  const auto date = base::Time::Now().InSecondsFSinceUnixEpoch();
  xmlNewTextChild(desc, nullptr, BAD_CAST "date",
                  BAD_CAST base::NumberToString(date).c_str());
  xmlNodePtr time = xmlNewChild(desc, nullptr, BAD_CAST "time", nullptr);
  xmlNewTextChild(time, nullptr, BAD_CAST "start",
                  BAD_CAST base::NumberToString(0).c_str());
  xmlNewTextChild(time, nullptr, BAD_CAST "end",
                  BAD_CAST base::NumberToString(end_ms).c_str());

  for (const auto& graphml_attr : brave_page_graph::GetGraphMLAttrs()) {
    graphml_attr.second->AddDefinitionNode(root);
  }

  xmlNodePtr graph_node = xmlNewChild(root, nullptr, BAD_CAST "graph", nullptr);
  xmlSetProp(graph_node, BAD_CAST "id", BAD_CAST "G");
  xmlSetProp(graph_node, BAD_CAST "edgedefault", BAD_CAST "directed");
  *out_graph_node = graph_node;
  return doc;
}

// Emits the header of a streamed GraphML file through save_ctxt/file: the
// <graphml> open literal, then the <desc>/<key> subtrees of header_doc via
// libxml, then the <graph> open literal — leaving <graph> open so callers can
// stream its children. Returns false on any write failure.
bool WriteStreamedHeaderThroughCtxt(xmlSaveCtxtPtr save_ctxt,
                                    base::File& file,
                                    xmlDocPtr header_doc,
                                    xmlNodePtr graph_node) {
  if (!WriteStreamLiteral(save_ctxt, file, kStreamHeaderLiteral)) {
    return false;
  }
  xmlNodePtr root = xmlDocGetRootElement(header_doc);
  for (xmlNodePtr child = root->children; child && child != graph_node;
       child = child->next) {
    xmlSaveTree(save_ctxt, child);
  }
  return WriteStreamLiteral(save_ctxt, file, kStreamGraphOpenLiteral);
}

}  // namespace

// Tier C durable record-time event log (opt-in via PAGEGRAPH_EVENT_LOG_DIR).
// Holds the append-only libxml save context + base::File and a persistent
// scratch <graph> parent whose children are unlinked after each item so live
// libxml memory stays O(one item). Its destructor releases libxml/file
// resources; PageGraph::~PageGraph()'s default is enough because of it.
struct PageGraphEventLog {
  base::File file;
  xmlSaveCtxtPtr save_ctxt = nullptr;
  xmlDocPtr scratch_doc = nullptr;
  xmlNodePtr scratch_parent = nullptr;
  std::string path;
  size_t emitted = 0;
  bool footer_written = false;

  ~PageGraphEventLog() {
    if (save_ctxt) {
      xmlSaveClose(save_ctxt);
    }
    if (scratch_doc) {
      xmlFreeDoc(scratch_doc);
    }
  }
};

// static
const char PageGraph::kSupplementName[] = "PageGraph";

// static
PageGraph* PageGraph::From(LocalFrame& frame) {
  return Supplement<LocalFrame>::From<PageGraph>(frame);
}

// static
void PageGraph::ProvideTo(LocalFrame& frame) {
  if (!base::FeatureList::IsEnabled(brave_page_graph::features::kPageGraph)) {
    return;
  }
  CHECK(!PageGraph::From(frame));
  CHECK(frame.IsLocalRoot());
  Page* page = frame.GetPage();
  CHECK(page);

  // Lookup the PageGraph from the initiator frame if it exists.
  const DOMNodeId initiator_dom_node_id =
      page->GetChromeClient().InitiatorDomNodeId();
  if (initiator_dom_node_id != kInvalidDOMNodeId) {
    blink::Node* initiator_node =
        blink::DOMNodeIds::NodeForId(initiator_dom_node_id);
    if (initiator_node) {
      auto* initiator_tree_page_graph = Supplement<LocalFrame>::From<PageGraph>(
          *initiator_node->TreeRoot().GetDocument().GetFrame());
      if (initiator_tree_page_graph) {
        frame.GetProbeSink()->AddPageGraph(initiator_tree_page_graph);
        return;
      }
    }
  }

  // Lookup the PageGraph from the related pages if it exists.
  for (const auto& related_page : page->RelatedPages()) {
    auto* main_local_frame = DynamicTo<LocalFrame>(related_page->MainFrame());
    if (!main_local_frame) {
      continue;
    }
    auto* related_page_graph =
        Supplement<LocalFrame>::From<PageGraph>(main_local_frame);
    if (related_page_graph) {
      frame.GetProbeSink()->AddPageGraph(related_page_graph);
      return;
    }
  }

  // Lookup the PageGraph from the ordinary pages if it exists.
  for (const auto& ordinary_page : page->OrdinaryPages()) {
    auto* main_local_frame = DynamicTo<LocalFrame>(ordinary_page->MainFrame());
    if (!main_local_frame) {
      continue;
    }
    auto* ordinary_page_graph =
        Supplement<LocalFrame>::From<PageGraph>(main_local_frame);
    if (ordinary_page_graph) {
      frame.GetProbeSink()->AddPageGraph(ordinary_page_graph);
      return;
    }
  }

  // Create a new PageGraph for the current frame.
  PageGraph* page_graph = MakeGarbageCollected<PageGraph>(frame);
  frame.GetProbeSink()->AddPageGraph(page_graph);
  Supplement<LocalFrame>::ProvideTo(frame, page_graph);
}

PageGraph::PageGraph(LocalFrame& local_frame)
    : Supplement<LocalFrame>(local_frame),
      frame_id_(GetFrameId(local_frame)),
      script_tracker_(this),
      request_tracker_(this) {
  CHECK(local_frame.IsLocalRoot());
  blink::Page* page = local_frame.GetPage();
  CHECK(page);

  if (!page->IsOrdinary()) {
    LOG(ERROR) << "PageGraph is ignoring non-ordinary page, some parts of the "
                  "web page may not be captured.";
    base::debug::DumpWithoutCrashing();
    return;
  }

  shields_node_ = AddNode<NodeShields>();
  ad_shield_node_ = AddNode<NodeShield>(brave_shields::kAds);
  tracker_shield_node_ = AddNode<NodeShield>(brave_shields::kTrackers);
  js_shield_node_ = AddNode<NodeShield>(brave_shields::kJavaScript);
  fingerprinting_shield_node_ =
      AddNode<NodeShield>(brave_shields::kFingerprintingV2);
  AddEdge<EdgeShield>(shields_node_, ad_shield_node_);
  AddEdge<EdgeShield>(shields_node_, tracker_shield_node_);
  AddEdge<EdgeShield>(shields_node_, js_shield_node_);
  AddEdge<EdgeShield>(shields_node_, fingerprinting_shield_node_);

  storage_node_ = AddNode<NodeStorageRoot>();
  cookie_jar_node_ = AddNode<NodeStorageCookieJar>();
  local_storage_node_ = AddNode<NodeStorageLocalStorage>();
  session_storage_node_ = AddNode<NodeStorageSessionStorage>();
  AddEdge<EdgeStorageBucket>(storage_node_, cookie_jar_node_);
  AddEdge<EdgeStorageBucket>(storage_node_, local_storage_node_);
  AddEdge<EdgeStorageBucket>(storage_node_, session_storage_node_);

  // Enable async call-stack recording so per-edge stack traces include the
  // async parent chain (timers, promises, fetch callbacks). Done here, before
  // any page script runs, so no async task is missed.
  if (auto* main_thread_debugger =
          blink::MainThreadDebugger::Instance(v8::Isolate::GetCurrent())) {
    brave_page_graph::EnsureAsyncStackCaptureEnabled(
        main_thread_debugger->ContextGroupId(&local_frame));
  }
}

PageGraph::~PageGraph() = default;

void PageGraph::Trace(blink::Visitor* visitor) const {
  Supplement<LocalFrame>::Trace(visitor);
  visitor->Trace(execution_context_nodes_);
  visitor->Trace(processed_js_urls_);
}

void PageGraph::NodeCreated(blink::Node* node) {
  DCHECK(!currently_constructed_nodes_.contains(node));
  currently_constructed_nodes_.emplace(node, false);
}

void PageGraph::RegisterPageGraphNodeFullyCreated(blink::Node* node) {
  auto node_it = currently_constructed_nodes_.find(node);
  if (node_it != currently_constructed_nodes_.end()) {
    const bool is_already_registered = node_it->second;
    currently_constructed_nodes_.erase(node_it);
    if (is_already_registered) {
      return;
    }
  }

  if (auto* document_node = DynamicTo<blink::Document>(node)) {
    RegisterDocumentNodeCreated(document_node);
    return;
  }

  if (auto* character_data_node = DynamicTo<blink::CharacterData>(node)) {
    RegisterHTMLTextNodeCreated(character_data_node);
    return;
  }

  RegisterHTMLElementNodeCreated(node);
}

void PageGraph::DidInsertDOMNode(blink::Node* node) {
  blink::Node* const parent = node->parentNode();
  if (!parent) {
    return;
  }

  if (IsA<blink::Document>(node)) {
    return;
  }

  blink::Node* const sibling = node->previousSibling();
  const blink::DOMNodeId sibling_node_id =
      sibling ? blink::DOMNodeIds::IdForNode(sibling) : 0;

  if (auto* character_data_node = DynamicTo<blink::CharacterData>(node)) {
    RegisterHTMLTextNodeInserted(character_data_node, parent, sibling_node_id);
    return;
  }

  RegisterHTMLElementNodeInserted(node, parent, sibling_node_id);
}

void PageGraph::WillRemoveDOMNode(blink::Node* node) {
  if (auto* character_data_node = DynamicTo<blink::CharacterData>(node)) {
    RegisterHTMLTextNodeRemoved(character_data_node);
    return;
  }

  RegisterHTMLElementNodeRemoved(node);
}

void PageGraph::DidModifyDOMAttr(blink::Element* element,
                                 const blink::QualifiedName& name,
                                 const AtomicString& value) {
  RegisterAttributeSet(element, name.ToString(), value);
}

void PageGraph::DidRemoveDOMAttr(blink::Element* element,
                                 const blink::QualifiedName& name) {
  RegisterAttributeDelete(element, name.ToString());
}

void PageGraph::DidCommitLoad(blink::LocalFrame* local_frame,
                              blink::DocumentLoader* loader) {
  blink::Document* document = local_frame->GetDocument();
  DCHECK(document);

  if (!document->IsHTMLDocument()) {
    VLOG(1) << "Skipping DidCommitLoad. !IsHTMLDocument()";
    return;
  }

  auto* node_domroot = To<NodeDOMRoot>(
      GetHTMLElementNode(blink::DOMNodeIds::IdForNode(document)));
  node_domroot->SetURL(document->Url());
  AssignSecurityOriginToNodeDOMRoot(document, node_domroot);
}

void PageGraph::WillSendNavigationRequest(uint64_t identifier,
                                          blink::DocumentLoader* loader,
                                          const blink::KURL& url,
                                          const AtomicString& http_method,
                                          blink::EncodedFormData*) {
  RegisterRequestStartForDocument(loader, identifier, url);
}

void PageGraph::WillSendRequest(
    blink::ExecutionContext* execution_context,
    blink::DocumentLoader* loader,
    const blink::KURL& fetch_context_url,
    const blink::ResourceRequest& request,
    const blink::ResourceResponse& redirect_response,
    const blink::ResourceLoaderOptions& options,
    blink::ResourceType resource_type,
    blink::RenderBlockingBehavior render_blocking_behavior,
    base::TimeTicks timestamp) {
  const FrameId frame_id = GetFrameId(execution_context);
  if (request.GetRedirectInfo()) {
    RegisterRequestRedirect(request, redirect_response, frame_id);
    return;
  }

  const String page_graph_resource_type = blink::Resource::ResourceTypeToString(
      resource_type, options.initiator_info.name);

  if (options.initiator_info.dom_node_id != blink::kInvalidDOMNodeId) {
    RegisterRequestStartFromElm(options.initiator_info.dom_node_id,
                                request.InspectorId(), frame_id, request.Url(),
                                page_graph_resource_type);
    return;
  }

  if (options.initiator_info.name == blink::fetch_initiator_type_names::kCSS ||
      options.initiator_info.name ==
          blink::fetch_initiator_type_names::kUacss ||
      options.initiator_info.name == blink::fetch_initiator_type_names::kLink ||
      resource_type == blink::ResourceType::kLinkPrefetch) {
    RegisterRequestStartFromCSSOrLink(loader, request.InspectorId(),
                                      request.Url(), page_graph_resource_type);
    return;
  }

  if (options.initiator_info.name ==
      blink::fetch_initiator_type_names::kFetch) {
    RegisterRequestStartFromCurrentScript(execution_context,
                                          request.InspectorId(), request.Url(),
                                          page_graph_resource_type);
    return;
  }

  if (options.initiator_info.name ==
      blink::fetch_initiator_type_names::kXmlhttprequest) {
    RegisterRequestStartFromCurrentScript(execution_context,
                                          request.InspectorId(), request.Url(),
                                          page_graph_resource_type);
    return;
  }

  if (options.initiator_info.name ==
      blink::fetch_initiator_type_names::kBeacon) {
    RegisterRequestStartFromCurrentScript(execution_context,
                                          request.InspectorId(), request.Url(),
                                          page_graph_resource_type);
    return;
  }

  if (options.initiator_info.name ==
          blink::fetch_initiator_type_names::kVideo ||
      options.initiator_info.name ==
          blink::fetch_initiator_type_names::kAudio) {
    RegisterRequestStartFromCSSOrLink(loader, request.InspectorId(),
                                      request.Url(), page_graph_resource_type);
    return;
  }

  if (options.initiator_info.name.empty()) {
    LOG(INFO) << "Empty request initiator for request id: "
              << request.InspectorId();
    ScriptId script_id = options.initiator_info.parent_script_id;
    if (script_id == 0)
      script_id = GetExecutingScriptId(execution_context);
    if (script_id) {
      RegisterRequestStartFromScript(execution_context, script_id,
                                     request.InspectorId(), request.Url(),
                                     page_graph_resource_type);
    } else {
      RegisterRequestStartFromCSSOrLink(loader, request.InspectorId(),
                                        request.Url(),
                                        page_graph_resource_type);
    }
    return;
  }

  LOG(ERROR) << "Unhandled request id: " << request.InspectorId()
             << " resource type: " << page_graph_resource_type
             << " url: " << request.Url() << "\n"
             << base::debug::StackTrace().ToString();
}

void PageGraph::DidReceiveData(uint64_t identifier,
                               blink::DocumentLoader*,
                               base::SpanOrSize<const char> data) {
  if (TrackedRequestRecord* request_record =
          request_tracker_.GetTrackingRecord(identifier)) {
    if (TrackedRequest* request = request_record->request.get()) {
      if (auto data_span = data.span()) {
        request->UpdateResponseBodyHash(*data_span);
      }
    }
    return;
  }

  if (request_tracker_.GetDocumentRequestInfo(identifier)) {
    // Track document data?
    return;
  }

  LOG(ERROR) << "DidReceiveData) untracked request id: " << identifier;
}

void PageGraph::DidReceiveBlob(uint64_t identifier,
                               blink::DocumentLoader*,
                               blink::BlobDataHandle*) {
  if (request_tracker_.GetTrackingRecord(identifier)) {
    // Track blob data?
    return;
  }

  // Document request doesn't trigger this event.

  LOG(ERROR) << "DidReceiveBlob) untracked request id: " << identifier;
}

void PageGraph::DidFinishLoading(uint64_t identifier,
                                 blink::DocumentLoader* loader,
                                 base::TimeTicks finish_time,
                                 int64_t encoded_data_length,
                                 int64_t decoded_body_length) {
  const FrameId frame_id = GetFrameId(loader);
  if (request_tracker_.GetTrackingRecord(identifier)) {
    RegisterRequestComplete(identifier, encoded_data_length, frame_id);
    return;
  }

  if (request_tracker_.GetDocumentRequestInfo(identifier)) {
    RegisterRequestCompleteForDocument(identifier, encoded_data_length,
                                       frame_id);
    return;
  }

  LOG(ERROR) << "DidFinishLoading) untracked request id: " << identifier;
}

void PageGraph::DidFailLoading(
    blink::CoreProbeSink* sink,
    uint64_t identifier,
    blink::DocumentLoader* loader,
    const blink::ResourceError&,
    const base::UnguessableToken& devtools_frame_or_worker_token) {
  const FrameId frame_id = GetFrameId(loader);
  if (request_tracker_.GetTrackingRecord(identifier)) {
    RegisterRequestError(identifier, frame_id);
    return;
  }

  if (request_tracker_.GetDocumentRequestInfo(identifier)) {
    RegisterRequestCompleteForDocument(identifier, -1, frame_id);
    return;
  }

  LOG(ERROR) << "DidFailLoading) untracked request id: " << identifier;
}

void PageGraph::ApplyCompilationModeOverride(
    const blink::ClassicScript& classic_script,
    v8::ScriptCompiler::CachedData**,
    v8::ScriptCompiler::CompileOptions* compile_options) {
  if (classic_script.SourceLocationType() !=
          ScriptSourceLocationType::kExternalFile ||
      classic_script.SourceUrl().IsEmpty()) {
    return;
  }
  // When PageGraph is active, always compile external scripts eagerly. We want
  // each DOM node to have its own script instance even if the underlying script
  // is fetched from the same URL.
  CHECK(compile_options);
  *compile_options = v8::ScriptCompiler::kEagerCompile;
}

void PageGraph::RegisterPageGraphScriptCompilation(
    blink::ExecutionContext* execution_context,
    const blink::ReferrerScriptInfo& referrer_info,
    const blink::ClassicScript& classic_script,
    v8::Local<v8::Script> script) {
  const ScriptId script_id = script->GetUnboundScript()->GetId();
  blink::KURL script_url = classic_script.BaseUrl();
  if (script_url.IsEmpty() || script_url.ProtocolIsAbout()) {
    script_url = classic_script.SourceUrl();
  }
  ScriptData script_data{
      .code = classic_script.SourceText().ToString(),
      .source = {
          .dom_node_id = referrer_info.GetDOMNodeId(),
          .parent_script_id = referrer_info.GetParentScriptId(),
          .url = script_url,
          .location_type = classic_script.SourceLocationType(),
      }};

  // Lookup parent script id for kJavascriptUrl scripts.
  if (script_data.source.location_type ==
          blink::ScriptSourceLocationType::kJavascriptUrl &&
      script_data.source.parent_script_id == 0) {
    auto processed_js_urls_it = processed_js_urls_.find(execution_context);

    if (processed_js_urls_it != processed_js_urls_.end()) {
      auto& processed_js_urls = processed_js_urls_it->value;
      const auto& processed_js_url_it =
          std::ranges::find(processed_js_urls, script_data.code,
                            &ProcessedJavascriptURL::script_code);

      if (processed_js_url_it != processed_js_urls.end()) {
        script_data.source.parent_script_id =
            processed_js_url_it->parent_script_id;
        processed_js_urls.erase(processed_js_url_it);
      }

      if (processed_js_urls.empty()) {
        processed_js_urls_.erase(processed_js_urls_it);
      }
    }
  }

  RegisterScriptCompilation(execution_context, script_id, script_data);
}

void PageGraph::RegisterPageGraphModuleCompilation(
    blink::ExecutionContext* execution_context,
    const blink::ReferrerScriptInfo& referrer_info,
    const blink::ModuleScriptCreationParams& params,
    v8::Local<v8::Module> module) {
  const ScriptId script_id = module->ScriptId();
  blink::KURL script_url = params.BaseURL();
  if (script_url.IsEmpty() || script_url.ProtocolIsAbout()) {
    script_url = params.SourceURL();
  }
  ScriptData script_data{
      .code = params.GetSourceText().ToString(),
      .source = {
          .dom_node_id = referrer_info.GetDOMNodeId(),
          .parent_script_id = referrer_info.GetParentScriptId(),
          .url = script_url,
          .is_module = true,
      }};

  RegisterScriptCompilation(execution_context, script_id, script_data);
}

void PageGraph::RegisterPageGraphScriptCompilationFromAttr(
    blink::EventTarget* event_target,
    const String& function_name,
    const String& script_body,
    v8::Local<v8::Function> compiled_function) {
  blink::Node* event_recipient = event_target->ToNode();
  if (!event_recipient && event_target->ToLocalDOMWindow() &&
      event_target->ToLocalDOMWindow()->document()) {
    event_recipient = event_target->ToLocalDOMWindow()->document()->body();
  }
  if (!event_recipient) {
    LOG(ERROR) << "No event_recipient for script from attribute";
    return;
  }
  const ScriptId script_id = compiled_function->ScriptId();
  ScriptData script_data{
      .code = script_body,
      .source = {
          .dom_node_id = blink::DOMNodeIds::IdForNode(event_recipient),
          .function_name = function_name,
      }};

  blink::ExecutionContext* context = event_recipient->GetExecutionContext();
  RegisterScriptCompilationFromAttr(context, script_id, script_data);
}

void PageGraph::RegisterPageGraphBindingEvent(
    blink::ExecutionContext* execution_context,
    const char* name,
    blink::PageGraphBindingType type,
    blink::PageGraphBindingEvent event) {
  // Not sure if bindings are required now given that WebAPI tracking does the
  // same job.
  // RegisterBindingEvent(execution_context, name, BindingTypeToString(type),
  //                      BindingEventToString(event));
}

void PageGraph::RegisterPageGraphWebAPICallWithResult(
    blink::ExecutionContext* execution_context,
    const char* name,
    const blink::PageGraphObject& receiver_data,
    const blink::PageGraphValues& args,
    const blink::ExceptionState* exception_state,
    const std::optional<blink::PageGraphValue>& result) {
  const std::string_view name_piece(name);
  if (name_piece.starts_with("Document.")) {
    if (name_piece == "Document.cookie.get") {
      RegisterStorageRead(execution_context,
                          String(*receiver_data.FindString("cookie_url")),
                          *result, brave_page_graph::StorageLocation::kCookie);
      return;
    } else if (name_piece == "Document.cookie.set") {
      String value(args[0].GetString());
      blink::Vector<String> cookie_structure = value.SplitSkippingEmpty('=');
      String cookie_key = *(cookie_structure.begin());
      String cookie_value =
          value.DeprecatedSubstring(cookie_key.length() + 1, value.length());
      RegisterStorageWrite(execution_context, cookie_key,
                           base::Value(cookie_value.Utf8()),
                           brave_page_graph::StorageLocation::kCookie,
                           brave_page_graph::CookieSource::kJS);
      return;
    }
  } else if (name_piece.starts_with("CookieStore.")) {
    // The async Cookie Store API. The instrumented call shape is
    // CookieStore.set(name, value) / CookieStore.delete(name); record it as a
    // cookie write tagged with the cookie-store source so it is distinguishable
    // from the document.cookie channel.
    // NOTE: the exact instrumented method name/arg shape must be confirmed
    // against the generated bindings; if it differs, this branch is inert and
    // the call falls through to a generic WebAPI call below.
    if (name_piece == "CookieStore.set" && args.size() >= 2) {
      RegisterStorageWrite(execution_context, String(args[0].GetString()),
                           args[1],
                           brave_page_graph::StorageLocation::kCookie,
                           brave_page_graph::CookieSource::kCookieStore);
      return;
    }
    if (name_piece == "CookieStore.delete" && args.size() >= 1) {
      RegisterStorageDelete(execution_context, String(args[0].GetString()),
                            brave_page_graph::StorageLocation::kCookie);
      return;
    }
  } else if (name_piece.starts_with("Storage.")) {
    String storage_type(*receiver_data.FindString("storage_type"));
    DCHECK(storage_type == "localStorage" || storage_type == "sessionStorage");
    const auto storage = storage_type == "localStorage"
                             ? StorageLocation::kLocalStorage
                             : StorageLocation::kSessionStorage;
    if (name_piece == "Storage.getItem") {
      DCHECK(result);
      RegisterStorageRead(execution_context, String(args[0].GetString()),
                          *result, storage);
      return;
    }
    if (name_piece == "Storage.setItem") {
      RegisterStorageWrite(execution_context, String(args[0].GetString()),
                           args[1], storage);
      return;
    }
    if (name_piece == "Storage.removeItem") {
      RegisterStorageDelete(execution_context, String(args[0].GetString()),
                            storage);
      return;
    }
    if (name_piece == "Storage.clear") {
      RegisterStorageClear(execution_context, storage);
      return;
    }
  }
  RegisterWebAPICall(execution_context, name, args);
  if (result)
    RegisterWebAPIResult(execution_context, name, *result);
}

void PageGraph::RegisterPageGraphEventListenerAdd(
    blink::EventTarget* event_target,
    const String& event_type,
    blink::RegisteredEventListener* registered_listener) {
  blink::Node* node = event_target->ToNode();
  if (!node || !node->IsHTMLElement()) {
    return;
  }
  const int listener_script_id =
      GetListenerScriptId(event_target, registered_listener->Callback());
  if (!listener_script_id) {
    return;
  }
  RegisterEventListenerAdd(node, event_type, registered_listener->Id(),
                           listener_script_id);
}

void PageGraph::RegisterPageGraphEventListenerRemove(
    blink::EventTarget* event_target,
    const String& event_type,
    blink::RegisteredEventListener* registered_listener) {
  blink::Node* node = event_target->ToNode();
  if (!node || !node->IsHTMLElement()) {
    return;
  }
  const int listener_script_id =
      GetListenerScriptId(event_target, registered_listener->Callback());
  if (!listener_script_id) {
    LOG(ERROR) << "No script id for event listener";
    return;
  }
  RegisterEventListenerRemove(node, event_type, registered_listener->Id(),
                              listener_script_id);
}

void PageGraph::RegisterPageGraphJavaScriptUrl(blink::Document* document,
                                               const KURL& url) {
  constexpr size_t kJavascriptSchemeLength = sizeof("javascript:") - 1;
  auto* execution_context = document->GetExecutionContext();

  processed_js_urls_.insert(execution_context, Vector<ProcessedJavascriptURL>())
      .stored_value->value.push_back(ProcessedJavascriptURL{
          .script_code =
              blink::DecodeUrlEscapeSequences(
                  url.GetString(), blink::DecodeUrlMode::kUtf8OrIsomorphic)
                  .DeprecatedSubstring(kJavascriptSchemeLength),
          .parent_script_id = GetExecutingScriptId(execution_context),
      });
}

void PageGraph::ConsoleMessageAdded(blink::ConsoleMessage* console_message) {
  blink::ExecutionContext* const execution_context =
      [&]() -> blink::ExecutionContext* {
    blink::LocalFrame* frame = console_message->Frame();
    blink::Document* document = frame ? frame->GetDocument() : nullptr;
    if (!document) {
      frame = GetSupplementable();
      document = frame->GetDocument();
      if (!document) {
        return nullptr;
      }
    }
    return document->GetExecutionContext();
  }();

  if (!execution_context) {
    return;
  }

  std::ostringstream str;
  base::DictValue dict;
  str << console_message->GetSource();
  dict.Set("source", str.str());
  str.str("");
  str << console_message->GetLevel();
  dict.Set("level", str.str());
  dict.Set("message", console_message->Message().Utf8());

  base::DictValue loc;
  loc.Set("url", console_message->Location()->Url().Utf8());
  loc.Set("line", static_cast<int>(console_message->Location()->LineNumber()));
  loc.Set("column",
          static_cast<int>(console_message->Location()->ColumnNumber()));
  loc.Set("script_id", console_message->Location()->ScriptId());
  dict.Set("location", std::move(loc));

  base::ListValue args;
  args.Append(std::move(dict));
  RegisterWebAPICall(execution_context, "ConsoleMessageAdded", std::move(args));
}

void PageGraph::RegisterV8ScriptCompilationFromEval(
    v8::Isolate* isolate,
    const int script_id,
    v8::Local<v8::String> source) {
  v8::page_graph::ExecutingScript executing_script =
      v8::page_graph::GetExecutingScript(isolate);
  ScriptData script_data{.code = blink::ToBlinkString<String>(
                             isolate, source, blink::kExternalize),
                         .source = {
                             .parent_script_id = executing_script.script_id,
                             .is_eval = true,
                         }};

  RegisterScriptCompilation(
      blink::ToExecutionContext(isolate->GetCurrentContext()), script_id,
      script_data);
}

void PageGraph::RegisterV8JSBuiltinCall(
    blink::ExecutionContext* receiver_context,
    const char* builtin_name,
    const blink::PageGraphValues& args,
    const std::optional<blink::PageGraphValue>& result) {
  RegisterJSBuiltInCall(receiver_context, builtin_name, args);
  if (result) {
    RegisterJSBuiltInResponse(receiver_context, builtin_name, *result);
  }
}

base::TimeTicks PageGraph::GetGraphStartTime() const {
  return elapsed_timer_.start_time();
}

GraphItemId PageGraph::GetNextGraphItemId() {
  return ++id_counter_;
}

void PageGraph::AddGraphItem(std::unique_ptr<GraphItem> graph_item) {
  GraphItem* item = graph_item.get();
  graph_items_.push_back(std::move(graph_item));

  // Tier C: mirror ToGraphML exactly by logging an item to the durable event
  // log only where it is retained — every node, and edges only if same-context
  // (below). No-op unless PAGEGRAPH_EVENT_LOG_DIR is set.
  EnsureEventLogOpen();

  if (auto* graph_node = DynamicTo<GraphNode>(item)) {
    nodes_.push_back(graph_node);
    LogGraphItem(graph_node);
    if (auto* element_node = DynamicTo<NodeHTMLElement>(graph_node)) {
      DCHECK(!element_nodes_.Contains(element_node->GetDOMNodeId()));
      element_nodes_.insert(element_node->GetDOMNodeId(), element_node);
    } else if (auto* text_node = DynamicTo<NodeHTMLText>(graph_node)) {
      DCHECK(!text_nodes_.Contains(text_node->GetDOMNodeId()));
      text_nodes_.insert(text_node->GetDOMNodeId(), text_node);
    } else if (auto* resource_node = DynamicTo<NodeResource>(graph_node)) {
      resource_nodes_.insert(resource_node->GetURL(), resource_node);
    } else if (auto* ad_filter_node = DynamicTo<NodeAdFilter>(graph_node)) {
      ad_filter_nodes_.insert(ad_filter_node->GetRule(), ad_filter_node);
    } else if (auto* tracker_filter_node =
                   DynamicTo<NodeTrackerFilter>(graph_node)) {
      tracker_filter_nodes_.insert(tracker_filter_node->GetHost(),
                                   tracker_filter_node);
    } else if (auto* fingerprinting_filter_node =
                   DynamicTo<NodeFingerprintingFilter>(graph_node)) {
      fingerprinting_filter_nodes_.emplace(
          fingerprinting_filter_node->GetRule(), fingerprinting_filter_node);
    } else if (auto* binding_node = DynamicTo<NodeBinding>(graph_node)) {
      binding_nodes_.insert(binding_node->GetBinding(), binding_node);
    } else if (auto* js_webapi_node = DynamicTo<NodeJSWebAPI>(graph_node)) {
      js_webapi_nodes_.insert(js_webapi_node->GetMethodName(), js_webapi_node);
    } else if (auto* js_builtin_node = DynamicTo<NodeJSBuiltin>(graph_node)) {
      js_builtin_nodes_.insert(js_builtin_node->GetMethodName(),
                               js_builtin_node);
    }
  } else if (auto* graph_edge = DynamicTo<GraphEdge>(item)) {
    // Record the JS call stack (sync + async) that produced this edge, if any.
    // Empty for edges created outside script execution (parser, network, engine
    // bookkeeping); GraphEdge omits the attribute in that case.
    graph_edge->SetStackTraceJson(brave_page_graph::CaptureStackTraceJson());
    // Connect only same-graph nodes. Multiple graphs can exist, but
    // interconnection is not implemented.
    if (graph_edge->GetInNode()->GetContext() ==
        graph_edge->GetOutNode()->GetContext()) {
      graph_edge->GetInNode()->AddInEdge(graph_edge);
      graph_edge->GetOutNode()->AddOutEdge(graph_edge);
      edges_.push_back(graph_edge);
      LogGraphItem(graph_edge);
    }
  } else {
    NOTREACHED();
  }
}

void PageGraph::GenerateReportForNode(const blink::DOMNodeId node_id,
                                      protocol::Array<String>& report) {
  const GraphNode* node;
  auto element_node_it = element_nodes_.find(node_id);
  if (element_node_it != element_nodes_.end()) {
    node = element_node_it->value;
  } else {
    auto text_node_it = text_nodes_.find(node_id);
    if (text_node_it != text_nodes_.end()) {
      node = text_node_it->value;
    } else {
      return;
    }
  }

  std::set<const GraphNode*> predecessors;
  std::set<const GraphNode*> successors;
  for (const auto* edge : edges_) {
    if (edge->GetInNode() == node)
      predecessors.insert(edge->GetOutNode());
    if (edge->GetOutNode() == node)
      successors.insert(edge->GetInNode());
  }

  for (const GraphNode* pred : predecessors) {
    if (IsA<NodeActor>(pred)) {
      for (const GraphEdge* edge : pred->GetOutEdges()) {
        if (edge->GetInNode() == node) {
          String reportItem(edge->GetItemDesc() +
                            "\r\n\r\nby: " + pred->GetItemDesc());
          report.push_back(String(reportItem));
        }
      }
    }
  }

  for (const GraphNode* succ : successors) {
    ItemName item_name = succ->GetItemName();
    if (item_name.starts_with("resource #")) {
      for (const GraphEdge* edge : succ->GetInEdges()) {
        String reportItem(edge->GetItemDesc() +
                          "\r\n\r\nby: " + edge->GetOutNode()->GetItemDesc());
        report.push_back(String(reportItem));
      }
    }
  }
}

String PageGraph::ToGraphML() const {
  // When the crawler sets PAGEGRAPH_OUT_DIR, stream the document straight to a
  // file on disk instead of materializing the whole graph as a single
  // blink::String. A blink::String's backing store caps at ~2GB, so very large
  // graphs (heavy sites over long dwells) overflow it and CHECK-crash the
  // renderer inside String::FromUtf8 (MaxElementCountInBackingStore).
  //
  // We serialize ONE graph item at a time straight to the file and never build
  // the whole GraphML document in memory: BuildStreamedHeaderDoc holds only the
  // <desc>/<key> definitions, and each <node>/<edge> subtree is emitted and then
  // freed immediately. This (a) keeps peak serialization memory at O(one item)
  // instead of a second full-graph copy (the libxml DOM tree), which was itself
  // a likely OOM/CHECK trigger on huge graphs, and (b) makes the on-disk file a
  // valid, monotonically-growing GraphML prefix (all <node>s, then all <edge>s):
  // if the renderer aborts mid-serialization the crawler can tail-repair the
  // truncated file and still recover every item written so far.
  //
  // We must NOT use xmlSaveFormatFileEnc / any libxml *Filename API here: Blink
  // replaces libxml's global I/O callbacks with document-scoped ones
  // (blink::OpenFunc), which CHECK-fail when invoked outside XML parsing (as we
  // are). Instead we drive the serializer with our own write callback into a
  // base::File: this streams incrementally (any size), never builds a giant
  // blink::String, and bypasses Blink's libxml callbacks entirely. Only the file
  // path (not the multi-GB graph) then crosses the DevTools pipe. The renderer
  // must be allowed to write the file, so the crawler runs with --no-sandbox.
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  const std::optional<std::string> out_dir = env->GetVar("PAGEGRAPH_OUT_DIR");
  if (out_dir && !out_dir->empty()) {
    const std::string file_path = base::StrCat(
        {*out_dir, "/pagegraph_", base::NumberToString(frame_id_), "_",
         base::NumberToString(base::Time::Now().InMillisecondsSinceUnixEpoch()),
         ".graphml"});
    base::File file(base::FilePath::FromUTF8Unsafe(file_path),
                    base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (!file.IsValid()) {
      LOG(ERROR) << "PageGraph: cannot open graphml output file " << file_path;
      return String();
    }
    // ioclose = nullptr: base::File owns the fd and closes it on scope exit.
    xmlSaveCtxtPtr save_ctxt = xmlSaveToIO(
        WriteFileCallback, /*ioclose=*/nullptr, &file, "UTF-8", /*options=*/0);
    if (!save_ctxt) {
      LOG(ERROR) << "PageGraph: cannot create xml save context for "
                 << file_path;
      return String();
    }

    xmlNodePtr graph_node = nullptr;
    xmlDocPtr header_doc = BuildStreamedHeaderDoc(
        frame_id_, IsRootFrame(), source_url_,
        elapsed_timer_.Elapsed().InMilliseconds(), &graph_node);
    bool write_ok =
        WriteStreamedHeaderThroughCtxt(save_ctxt, file, header_doc, graph_node);

    // Stream every node then every edge, one item at a time, draining the
    // scratch <graph> parent after each so live libxml memory stays O(1 item).
    constexpr size_t kFlushEvery = 2000;
    size_t emitted = 0;
    const auto emit_item = [&](const auto* item) {
      if (!write_ok) {
        return;
      }
      item->AddGraphMLTag(header_doc, graph_node);
      for (xmlNodePtr child = graph_node->children; child;) {
        xmlNodePtr next = child->next;
        xmlSaveTree(save_ctxt, child);
        xmlUnlinkNode(child);
        xmlFreeNode(child);
        child = next;
      }
      if (++emitted % kFlushEvery == 0) {
        xmlSaveFlush(save_ctxt);
      }
    };
    for (const auto* node : nodes_) {
      emit_item(node);
    }
    for (const auto* edge : edges_) {
      emit_item(edge);
    }

    if (write_ok) {
      write_ok = WriteStreamLiteral(save_ctxt, file, kStreamFooterLiteral);
    }
    const int flush_result = xmlSaveFlush(save_ctxt);
    xmlSaveClose(save_ctxt);
    xmlFreeDoc(header_doc);

    // The graph serialized cleanly, so any Tier C event log for this frame is
    // now redundant; finalize it so the crawler deletes it on the happy path.
    CloseEventLog();

    if (!write_ok || flush_result == -1) {
      LOG(ERROR) << "PageGraph: failed to serialize graphml to " << file_path;
      return String();
    }
    return String::FromUtf8(file_path);
  }

  // Fallback (PAGEGRAPH_OUT_DIR unset): materialize the whole graph inline and
  // return it as before. This path still caps at ~2GB and is only safe for
  // small graphs.
  xmlNodePtr graph_node = nullptr;
  xmlDocPtr graphml_doc = BuildStreamedHeaderDoc(
      frame_id_, IsRootFrame(), source_url_,
      elapsed_timer_.Elapsed().InMilliseconds(), &graph_node);
  for (const auto* node : nodes_) {
    node->AddGraphMLTag(graphml_doc, graph_node);
  }
  for (const auto* edge : edges_) {
    edge->AddGraphMLTag(graphml_doc, graph_node);
  }
  xmlChar* xml_string;
  int size;
  xmlDocDumpMemoryEnc(graphml_doc, &xml_string, &size, "UTF-8");
  // SAFETY: unfortunately xmlDocDumpMemoryEnc is a C api that
  // manages the buffer internally, so we have to rely on it for the
  // pointer/size pair.
  auto graphml_string = String::FromUtf8(base::as_bytes(UNSAFE_BUFFERS(
      base::span(xml_string, base::checked_cast<size_t>(size)))));
  DCHECK(!graphml_string.empty());

  xmlFree(xml_string);
  xmlFree(graphml_doc);

  return graphml_string;
}

// Tier C: open the record-time event log lazily on the first AddGraphItem when
// PAGEGRAPH_EVENT_LOG_DIR is set. Writes the same GraphML header as the streamed
// ToGraphML path, then leaves <graph> open so LogGraphItem can append items as
// they are recorded. Any failure latches event_log_disabled_ — the log is a
// best-effort safety net and must never take down a crawl.
void PageGraph::EnsureEventLogOpen() {
  if (event_log_ || event_log_disabled_) {
    return;
  }
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  const std::optional<std::string> dir = env->GetVar("PAGEGRAPH_EVENT_LOG_DIR");
  if (!dir || dir->empty()) {
    event_log_disabled_ = true;
    return;
  }

  auto log = std::make_unique<PageGraphEventLog>();
  log->path = base::StrCat(
      {*dir, "/pagegraph_eventlog_", base::NumberToString(frame_id_), "_",
       base::NumberToString(base::Time::Now().InMillisecondsSinceUnixEpoch()),
       ".graphml.partial"});
  log->file = base::File(base::FilePath::FromUTF8Unsafe(log->path),
                         base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!log->file.IsValid()) {
    LOG(ERROR) << "PageGraph: cannot open event log " << log->path;
    event_log_disabled_ = true;
    return;
  }
  log->save_ctxt = xmlSaveToIO(WriteFileCallback, /*ioclose=*/nullptr,
                               &log->file, "UTF-8", /*options=*/0);
  if (!log->save_ctxt) {
    LOG(ERROR) << "PageGraph: cannot create event-log save context for "
               << log->path;
    event_log_disabled_ = true;
    return;
  }

  xmlNodePtr header_graph_node = nullptr;
  xmlDocPtr header_doc = BuildStreamedHeaderDoc(
      frame_id_, IsRootFrame(), source_url_,
      elapsed_timer_.Elapsed().InMilliseconds(), &header_graph_node);
  const bool header_ok = WriteStreamedHeaderThroughCtxt(
      log->save_ctxt, log->file, header_doc, header_graph_node);
  xmlFreeDoc(header_doc);
  if (!header_ok) {
    LOG(ERROR) << "PageGraph: cannot write event-log header " << log->path;
    event_log_disabled_ = true;  // ~PageGraphEventLog closes save_ctxt/file.
    return;
  }

  // Persistent detached scratch <graph> parent: LogGraphItem builds each item's
  // subtree under it, serializes it, then unlinks/frees the child, so live
  // libxml memory stays O(one item) for the whole recording.
  log->scratch_doc = xmlNewDoc(BAD_CAST "1.0");
  log->scratch_parent = xmlNewNode(nullptr, BAD_CAST "graph");
  xmlDocSetRootElement(log->scratch_doc, log->scratch_parent);

  VLOG(1) << "PageGraph: event log opened at " << log->path;
  event_log_ = std::move(log);
}

// Tier C: serialize one graph item to the event log as it is recorded. No-op if
// the log is disabled/closed. Flushes periodically so a hard renderer crash
// loses at most the last handful of items (write() bytes already reach the OS
// page cache, which survives the crash).
void PageGraph::LogGraphItem(const GraphItem* item) {
  if (!event_log_ || event_log_->footer_written) {
    return;
  }
  PageGraphEventLog& log = *event_log_;
  item->AddGraphMLTag(log.scratch_doc, log.scratch_parent);
  for (xmlNodePtr child = log.scratch_parent->children; child;) {
    xmlNodePtr next = child->next;
    xmlSaveTree(log.save_ctxt, child);
    xmlUnlinkNode(child);
    xmlFreeNode(child);
    child = next;
  }
  constexpr size_t kEventLogFlushEvery = 64;
  if (++log.emitted % kEventLogFlushEvery == 0) {
    xmlSaveFlush(log.save_ctxt);
  }
}

// Tier C: finalize the event log after a clean ToGraphML. Appends the footer so
// the file is well-formed, then flushes; ~PageGraphEventLog does the actual
// xmlSaveClose/free. The crawler deletes this now-redundant .partial on the
// happy path. On a crash this is never reached, leaving a tail-repairable
// prefix for recovery.
void PageGraph::CloseEventLog() const {
  if (!event_log_ || event_log_->footer_written) {
    return;
  }
  PageGraphEventLog& log = *event_log_;
  log.footer_written = true;
  WriteStreamLiteral(log.save_ctxt, log.file, kStreamFooterLiteral);
  xmlSaveFlush(log.save_ctxt);
}

NodeHTML* PageGraph::GetHTMLNode(const DOMNodeId node_id) const {
  VLOG(1) << "GetHTMLNode) node id: " << node_id;
  auto element_node_it = element_nodes_.find(node_id);
  if (element_node_it != element_nodes_.end()) {
    return element_node_it->value;
  }
  auto text_node_it = text_nodes_.find(node_id);
  CHECK(text_node_it != text_nodes_.end()) << "HTMLNode not found: " << node_id;
  return text_node_it->value;
}

NodeHTMLElement* PageGraph::GetHTMLElementNode(
    std::variant<blink::DOMNodeId, blink::Node*> node_var) {
  blink::DOMNodeId node_id;
  blink::Node* node = nullptr;
  if (std::holds_alternative<blink::DOMNodeId>(node_var)) {
    node_id = std::get<blink::DOMNodeId>(node_var);
  } else {
    node = std::get<blink::Node*>(node_var);
    node_id = blink::DOMNodeIds::IdForNode(node);
  }

  // This function uses node_id in most scenarios, because a node is already
  // registered in 99.9% of calls. A single lookup in element_nodes_ is all we
  // need.
  VLOG(1) << "GetHTMLElementNode) node id: " << node_id;
  auto element_node_it = element_nodes_.find(node_id);
  if (element_node_it != element_nodes_.end()) {
    return element_node_it->value;
  }

  // We can get here when a node constructor triggers a synchronous
  // WillSendRequest or RegisterAttributeSet event which PG should handle, but
  // because the node is not fully constructed yet, we need to register it
  // preemptively at this point.
  if (!node) {
    DCHECK(std::holds_alternative<blink::DOMNodeId>(node_var));
    node = blink::DOMNodeIds::NodeForId(node_id);
    CHECK(node);
  }
  if (RegisterCurrentlyConstructedNode(node)) {
    element_node_it = element_nodes_.find(node_id);
    if (element_node_it != element_nodes_.end()) {
      return element_node_it->value;
    }
  }

  // If a node is not found at this point, then something is wrong and there
  // might be another edge case we need to handle.
  NOTREACHED() << "HTMLElementNode not found: " << node_id;
}

NodeHTMLText* PageGraph::GetHTMLTextNode(const DOMNodeId node_id) const {
  auto text_node_it = text_nodes_.find(node_id);
  CHECK(text_node_it != text_nodes_.end())
      << "HTMLTextNode not found: " << node_id;
  return text_node_it->value;
}

bool PageGraph::RegisterCurrentlyConstructedNode(blink::Node* node) {
  auto node_it = currently_constructed_nodes_.find(node);
  if (node_it == currently_constructed_nodes_.end()) {
    // Node is not being constructed currently.
    return false;
  }
  if (node_it->second) {
    // Node is already registered.
    return false;
  }

  RegisterPageGraphNodeFullyCreated(node);
  // Node should be removed from currently_constructed_nodes_.
  DCHECK(!currently_constructed_nodes_.contains(node));
  // Mark the node as an already registered for the upcoming
  // RegisterPageGraphNodeFullyCreated call.
  currently_constructed_nodes_.emplace(node, true);
  return true;
}

void PageGraph::RegisterDocumentNodeCreated(blink::Document* document) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(document);
  blink::ExecutionContext* execution_context = document->GetExecutionContext();
  bool is_frame_attached = document->GetFrame() != nullptr;

  blink::HTMLFrameOwnerElement* owner = document->LocalOwner();
  FrameId frame_id = GetFrameId(document);
  if (blink::Document* parent_document = document->ParentDocument()) {
    frame_id = GetFrameId(parent_document->GetExecutionContext());
  }

  VLOG(1) << "RegisterDocumentNodeCreated) document id: " << node_id
          << ", execution context: " << execution_context
          << ", is frame attached: " << is_frame_attached
          << ", frame id: " << frame_id;

  v8::Isolate* const isolate = v8::Isolate::GetCurrent();
  if (isolate) {
    static base::NoDestructor<V8PageGraphDelegate> page_graph_delegate;
    v8::page_graph::SetPageGraphDelegate(isolate, page_graph_delegate.get());
  }

  const String local_tag_name(static_cast<blink::Node*>(document)->nodeName());
  auto* dom_root =
      AddNode<NodeDOMRoot>(node_id, local_tag_name, is_frame_attached);
  auto url = document->Url();
  dom_root->SetURL(url);
  if (!source_url_ && url.IsValid() && url.ProtocolIsInHttpFamily()) {
    source_url_ = url;
  }

  AssignSecurityOriginToNodeDOMRoot(document, dom_root);

  auto execution_context_nodes_it =
      execution_context_nodes_.find(execution_context);
  if (execution_context_nodes_it == execution_context_nodes_.end()) {
    ExecutionContextNodes nodes{
        .parser_node = AddNode<NodeParser>(),
        .extensions_node = AddNode<NodeExtensions>(),
    };
    AddEdge<EdgeStructure>(nodes.parser_node, nodes.extensions_node);
    AddEdge<EdgeStructure>(nodes.parser_node, dom_root);
    execution_context_nodes_.insert(execution_context, std::move(nodes));
  }

  if (owner) {
    NodeHTMLElement* owner_graph_node = GetHTMLElementNode(owner);
    AddEdge<EdgeCrossDOM>(To<NodeFrameOwner>(owner_graph_node), dom_root);
  }

  AddEdge<EdgeNodeCreate>(GetCurrentActingNode(execution_context), dom_root,
                          frame_id);
}

void PageGraph::RegisterHTMLTextNodeCreated(blink::CharacterData* node) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);

  VLOG(1) << "RegisterHTMLTextNodeCreated) node id: " << node_id
          << ", text"; /*: '" + local_text + "'"*/
  NodeActor* const acting_node =
      GetCurrentActingNode(node->GetExecutionContext());

  FrameId frame_id = GetFrameId(node);
  NodeHTMLText* const new_node = AddNode<NodeHTMLText>(node_id, node->data());
  AddEdge<EdgeNodeCreate>(acting_node, new_node, frame_id);
}

void PageGraph::RegisterHTMLElementNodeCreated(blink::Node* node) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);
  String local_tag_name = node->nodeName();
  FrameId frame_id = GetFrameId(node);

  VLOG(1) << "RegisterHTMLElementNodeCreated) node id: " << node_id << " ("
          << local_tag_name << ") " << ", frame id: " << frame_id;
  NodeActor* const acting_node =
      GetCurrentActingNode(node->GetExecutionContext());

  NodeHTMLElement* new_node = nullptr;
  if (node->IsFrameOwnerElement()) {
    new_node = AddNode<NodeFrameOwner>(node_id, local_tag_name);
  } else {
    new_node = AddNode<NodeHTMLElement>(node_id, local_tag_name);
  }

  AddEdge<EdgeNodeCreate>(acting_node, new_node, frame_id);
}

void PageGraph::RegisterHTMLTextNodeInserted(
    blink::CharacterData* node,
    blink::Node* parent_node,
    const DOMNodeId before_sibling_id) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);
  const blink::DOMNodeId parent_node_id =
      blink::DOMNodeIds::IdForNode(parent_node);

  FrameId frame_id = GetFrameId(parent_node);

  VLOG(1) << "RegisterHTMLTextNodeInserted) node id: " << node_id
          << ", parent id: " << parent_node_id
          << ", prev sibling id: " << before_sibling_id
          << ", frame id: " << frame_id;

  NodeActor* const acting_node =
      GetCurrentActingNode(node->GetExecutionContext());

  NodeHTMLElement* parent_graph_node =
      parent_node ? GetHTMLElementNode(parent_node) : nullptr;
  NodeHTML* prior_graph_sibling_node =
      before_sibling_id ? GetHTMLNode(before_sibling_id) : nullptr;
  NodeHTMLText* const inserted_node = GetHTMLTextNode(node_id);

  AddEdge<EdgeNodeInsert>(acting_node, inserted_node, frame_id,
                          parent_graph_node, prior_graph_sibling_node);
}

void PageGraph::RegisterHTMLElementNodeInserted(
    blink::Node* node,
    blink::Node* parent_node,
    const DOMNodeId before_sibling_id) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);
  const blink::DOMNodeId parent_node_id =
      blink::DOMNodeIds::IdForNode(parent_node);

  FrameId frame_id = GetFrameId(parent_node);

  VLOG(1) << "RegisterHTMLElementNodeInserted) node id: " << node_id
          << ", parent node id: " << parent_node_id
          << ", prev sibling id: " << before_sibling_id
          << ", frame id: " << frame_id;
  NodeActor* const acting_node =
      GetCurrentActingNode(node->GetExecutionContext());

  NodeHTMLElement* parent_graph_node =
      parent_node ? GetHTMLElementNode(parent_node) : nullptr;
  NodeHTML* prior_graph_sibling_node =
      before_sibling_id ? GetHTMLNode(before_sibling_id) : nullptr;
  NodeHTMLElement* const inserted_node = GetHTMLElementNode(node_id);

  AddEdge<EdgeNodeInsert>(acting_node, inserted_node, frame_id,
                          parent_graph_node, prior_graph_sibling_node);

  // If this node is being inserted by the parser, then it may have attributes
  // created by the parser as well, that we need to register.
  // Also, we need to add any attributes that are on the node
  // at creation time. This will happen if there the node is being
  // created by the parser and has inline attributes in the text.
  if (acting_node->IsNodeParser()) {
    blink::Element* element = DynamicTo<blink::Element>(node);
    if (element && element->hasAttributes()) {
      for (auto&& attr : element->Attributes()) {
        const String attr_name = attr.GetName().ToString();
        const String attr_value = attr.Value();
        AddEdge<EdgeAttributeSet>(acting_node, inserted_node, frame_id,
                                  attr_name, attr_value, false);
      }
    }
  }
}

void PageGraph::RegisterHTMLTextNodeRemoved(blink::CharacterData* node) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);
  VLOG(1) << "RegisterHTMLTextNodeRemoved) node id: " << node_id;
  NodeActor* const acting_node =
      GetCurrentActingNode(node->GetExecutionContext());

  FrameId frame_id = GetFrameId(node);
  NodeHTMLText* const removed_node = GetHTMLTextNode(node_id);
  AddEdge<EdgeNodeRemove>(static_cast<NodeScriptLocal*>(acting_node),
                          removed_node, frame_id);
}

void PageGraph::RegisterHTMLElementNodeRemoved(blink::Node* node) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);
  VLOG(1) << "RegisterHTMLElementNodeRemoved) node id: " << node_id;
  NodeActor* const acting_node =
      GetCurrentActingNode(node->GetExecutionContext());

  FrameId frame_id = GetFrameId(node);
  NodeHTMLElement* const removed_node = GetHTMLElementNode(node_id);
  AddEdge<EdgeNodeRemove>(static_cast<NodeScriptLocal*>(acting_node),
                          removed_node, frame_id);
}

void PageGraph::RegisterEventListenerAdd(blink::Node* node,
                                         const String& event_type,
                                         const EventListenerId listener_id,
                                         ScriptId listener_script_id) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);

  VLOG(1) << "RegisterEventListenerAdd) node id: " << node_id
          << ", event_type: " << event_type << ", listener_id: " << listener_id
          << ", listener_script_id: " << listener_script_id;
  NodeActor* const acting_node =
      GetCurrentActingNode(node->GetExecutionContext());

  NodeHTMLElement* const element_node = GetHTMLElementNode(node);
  FrameId frame_id = GetFrameId(node);
  AddEdge<EdgeEventListenerAdd>(
      acting_node, element_node, frame_id, event_type, listener_id,
      script_tracker_.GetScriptNode(v8::Isolate::GetCurrent(),
                                    listener_script_id));
}

void PageGraph::RegisterEventListenerRemove(blink::Node* node,
                                            const String& event_type,
                                            const EventListenerId listener_id,
                                            ScriptId listener_script_id) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);

  VLOG(1) << "RegisterEventListenerRemove) node id: " << node_id
          << ", event_type: " << event_type << ", listener_id: " << listener_id
          << ", listener_script_id: " << listener_script_id;
  NodeActor* const acting_node =
      GetCurrentActingNode(node->GetExecutionContext());

  NodeHTMLElement* const element_node = GetHTMLElementNode(node);
  FrameId frame_id = GetFrameId(node);
  AddEdge<EdgeEventListenerRemove>(
      acting_node, element_node, frame_id, event_type, listener_id,
      script_tracker_.GetScriptNode(v8::Isolate::GetCurrent(),
                                    listener_script_id));
}

void PageGraph::RegisterInlineStyleSet(blink::Element* element,
                                       const String& attr_name,
                                       const String& attr_value) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(element);

  VLOG(1) << "RegisterInlineStyleSet) node id: " << node_id
          << ", attr: " << attr_name << ", value: " << attr_value;
  NodeActor* const acting_node =
      GetCurrentActingNode(element->GetExecutionContext());

  NodeHTMLElement* const target_node = GetHTMLElementNode(element);
  FrameId frame_id = GetFrameId(element);
  AddEdge<EdgeAttributeSet>(acting_node, target_node, frame_id, attr_name,
                            attr_value, true);
}

void PageGraph::RegisterInlineStyleDelete(blink::Element* element,
                                          const String& attr_name) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(element);

  VLOG(1) << "RegisterInlineStyleDelete) node id: " << node_id
          << ", attr: " << attr_name;
  NodeActor* const acting_node =
      GetCurrentActingNode(element->GetExecutionContext());

  NodeHTMLElement* const target_node = GetHTMLElementNode(element);
  FrameId frame_id = GetFrameId(element);
  AddEdge<EdgeAttributeDelete>(acting_node, target_node, frame_id, attr_name,
                               true);
}

void PageGraph::RegisterAttributeSet(blink::Element* element,
                                     const String& attr_name,
                                     const String& attr_value) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(element);

  VLOG(1) << "RegisterAttributeSet) node id: " << node_id
          << ", attr: " << attr_name << ", value: " << attr_value;

  NodeActor* const acting_node =
      GetCurrentActingNode(element->GetExecutionContext());

  NodeHTMLElement* const target_node = GetHTMLElementNode(element);
  FrameId frame_id = GetFrameId(element);
  AddEdge<EdgeAttributeSet>(acting_node, target_node, frame_id, attr_name,
                            attr_value);
}

void PageGraph::RegisterAttributeDelete(blink::Element* element,
                                        const String& attr_name) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(element);

  VLOG(1) << "RegisterAttributeDelete) node id: " << node_id
          << ", attr: " << attr_name;
  NodeActor* const acting_node =
      GetCurrentActingNode(element->GetExecutionContext());

  NodeHTMLElement* const target_node = GetHTMLElementNode(element);
  FrameId frame_id = GetFrameId(element);
  AddEdge<EdgeAttributeDelete>(acting_node, target_node, frame_id, attr_name);
}

void PageGraph::RegisterTextNodeChange(blink::CharacterData* node,
                                       const String& new_text) {
  const blink::DOMNodeId node_id = blink::DOMNodeIds::IdForNode(node);
  VLOG(1) << "RegisterNewTextNodeText) node id: " << node_id;
  NodeScriptLocal* const acting_node = static_cast<NodeScriptLocal*>(
      GetCurrentActingNode(node->GetExecutionContext()));

  NodeHTMLText* const text_node = GetHTMLTextNode(node_id);
  AddEdge<EdgeTextChange>(acting_node, text_node, new_text);
}

void PageGraph::DoRegisterRequestStart(const InspectorId request_id,
                                       GraphNode* requesting_node,
                                       const FrameId& frame_id,
                                       const KURL& local_url,
                                       const String& resource_type) {
  NodeResource* const requested_node = GetResourceNodeForUrl(local_url);

  scoped_refptr<const TrackedRequestRecord> request_record =
      request_tracker_.RegisterRequestStart(
          request_id, requesting_node, frame_id, requested_node, resource_type);
}

void PageGraph::RegisterRequestStartFromElm(const DOMNodeId node_id,
                                            const InspectorId request_id,
                                            const FrameId& frame_id,
                                            const KURL& url,
                                            const String& resource_type) {
  // For now, explode if we're getting duplicate requests for the same
  // URL in the same document.  This might need to be changed.
  VLOG(1) << "RegisterRequestStartFromElm) node id: " << node_id
          << ", request id: " << request_id << ", frame id: " << frame_id
          << ", url: " << url << ", type: " << resource_type;

  // We should know about the node thats issuing the request.
  NodeHTMLElement* const requesting_node = GetHTMLElementNode(node_id);
  DoRegisterRequestStart(request_id, requesting_node, frame_id, url,
                         resource_type);
}

void PageGraph::RegisterRequestStartFromCurrentScript(
    blink::ExecutionContext* execution_context,
    const InspectorId request_id,
    const KURL& url,
    const String& resource_type) {
  VLOG(1) << "RegisterRequestStartFromCurrentScript)";
  const ScriptId script_id = GetExecutingScriptId(execution_context);
  RegisterRequestStartFromScript(execution_context, script_id, request_id, url,
                                 resource_type);
}

void PageGraph::RegisterRequestStartFromScript(
    blink::ExecutionContext* execution_context,
    const ScriptId script_id,
    const InspectorId request_id,
    const blink::KURL& url,
    const String& resource_type) {
  VLOG(1) << "RegisterRequestStartFromScript) script id: " << script_id
          << " request id: " << request_id << ", url: " << url
          << ", type: " << resource_type;
  NodeActor* const acting_node =
      script_tracker_.GetScriptNode(v8::Isolate::GetCurrent(), script_id);
  FrameId frame_id = GetFrameId(execution_context);
  DoRegisterRequestStart(request_id, acting_node, frame_id, url, resource_type);
}

// This is basically the same as |RegisterRequestStartFromCurrentScript|,
// except we don't require the acting node to be a script (CSS fetches
// can be initiated by the parser).
void PageGraph::RegisterRequestStartFromCSSOrLink(blink::DocumentLoader* loader,
                                                  const InspectorId request_id,
                                                  const blink::KURL& url,
                                                  const String& resource_type) {
  blink::ExecutionContext* execution_context =
      loader->GetFrame()->GetDocument()->GetExecutionContext();
  NodeActor* const acting_node = GetCurrentActingNode(execution_context);

  FrameId frame_id = GetFrameId(execution_context);
  if (IsA<NodeParser>(acting_node)) {
    VLOG(1) << "RegisterRequestStartFromCSSOrLink) request id: " << request_id
            << ", frame id: " << frame_id << ", url: " << url
            << ", type: " << resource_type;
  } else {
    VLOG(1) << "RegisterRequestStartFromCSSOrLink) script id: "
            << static_cast<NodeScript*>(acting_node)->GetScriptId()
            << ", request id: " << request_id << ", url: " << url
            << ", type: " << resource_type;
  }

  DoRegisterRequestStart(request_id, acting_node, frame_id, url, resource_type);
}

// Request start for root document and subdocument HTML
void PageGraph::RegisterRequestStartForDocument(blink::DocumentLoader* loader,
                                                const InspectorId request_id,
                                                const blink::KURL& url) {
  blink::LocalFrame* frame = loader->GetFrame();
  CHECK(frame);
  bool is_main_frame = frame->IsMainFrame();
  const FrameId frame_id = GetFrameId(*frame);
  const auto timestamp = elapsed_timer_.Elapsed();

  VLOG(1) << "RegisterRequestStartForDocument) frame id: " << frame_id
          << ", request id: " << request_id << ", url: " << url
          << ", is_main_frame: " << is_main_frame;

  request_tracker_.RegisterDocumentRequestStart(request_id, frame_id, url,
                                                is_main_frame, timestamp);
}

void PageGraph::RegisterRequestRedirect(
    const ResourceRequest& request,
    const ResourceResponse& redirect_response,
    const FrameId& frame_id) {
  NodeResource* const requested_node = GetResourceNodeForUrl(request.Url());

  request_tracker_.RegisterRequestRedirect(request.InspectorId(), frame_id,
                                           request.Url(), redirect_response,
                                           requested_node);
}

void PageGraph::RegisterRequestComplete(const InspectorId request_id,
                                        int64_t encoded_data_length,
                                        const FrameId& frame_id) {
  VLOG(1) << "RegisterRequestComplete) request id: " << request_id;

  scoped_refptr<const TrackedRequestRecord> request_record =
      request_tracker_.RegisterRequestComplete(request_id, encoded_data_length,
                                               frame_id);
}

void PageGraph::RegisterRequestCompleteForDocument(
    const InspectorId request_id,
    const int64_t encoded_data_length,
    const FrameId& frame_id) {
  VLOG(1) << "RegisterRequestCompleteForDocument) request id: " << request_id
          << ", frame id: " << frame_id
          << ", encoded_data_length: " << encoded_data_length;

  const auto timestamp = elapsed_timer_.Elapsed();
  request_tracker_.RegisterDocumentRequestComplete(
      request_id, frame_id, encoded_data_length, timestamp);
}

void PageGraph::RegisterRequestError(const InspectorId request_id,
                                     const FrameId& frame_id) {
  VLOG(1) << "RegisterRequestError) request id: " << request_id;

  scoped_refptr<const TrackedRequestRecord> request_record =
      request_tracker_.RegisterRequestError(request_id, frame_id);
}

void PageGraph::RegisterResourceBlockAd(const blink::WebURL& url,
                                        const String& rule) {
  VLOG(1) << "RegisterResourceBlockAd) url: " << url << ", rule: " << rule;

  NodeResource* const resource_node = GetResourceNodeForUrl(url);
  NodeAdFilter* const filter_node = GetAdFilterNodeForRule(rule);

  AddEdge<EdgeResourceBlock>(filter_node, resource_node);
}

void PageGraph::RegisterResourceBlockTracker(const blink::WebURL& url,
                                             const String& host) {
  VLOG(1) << "RegisterResourceBlockTracker) url: " << url << ", host: " << host;

  NodeResource* const resource_node = GetResourceNodeForUrl(url);
  NodeTrackerFilter* const filter_node = GetTrackerFilterNodeForHost(host);

  AddEdge<EdgeResourceBlock>(filter_node, resource_node);
}

void PageGraph::RegisterResourceBlockJavaScript(const blink::WebURL& url) {
  VLOG(1) << "RegisterResourceBlockJavaScript) url: " << url;

  NodeResource* const resource_node = GetResourceNodeForUrl(url);
  AddEdge<EdgeResourceBlock>(js_shield_node_, resource_node);
}

void PageGraph::RegisterResourceBlockFingerprinting(
    const blink::WebURL& url,
    const FingerprintingRule& rule) {
  VLOG(1) << "RegisterResourceBlockFingerprinting) url: " << url
          << ", rule: " << rule.ToString();

  NodeResource* const resource_node = GetResourceNodeForUrl(url);
  NodeFingerprintingFilter* const filter_node =
      GetFingerprintingFilterNodeForRule(rule);

  AddEdge<EdgeResourceBlock>(filter_node, resource_node);
}

void PageGraph::RegisterScriptCompilation(
    blink::ExecutionContext* execution_context,
    const ScriptId script_id,
    const ScriptData& script_data) {
  FrameId frame_id = GetFrameId(execution_context);
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  VLOG(1) << "RegisterScriptCompilation) script id: " << script_id
          << ", frame id: " << frame_id << ", location: "
          << static_cast<int>(script_data.source.location_type)
          << ", parent script id: " << script_data.source.parent_script_id
          << ", script: \n"
          << (VLOG_IS_ON(2) ? script_data.code : String("<VLOG(2)>"));

  NodeScriptLocal* const code_node =
      script_tracker_.AddScriptNode(isolate, script_id, script_data);
  if (script_data.source.is_module) {
    // Module scripts are pulled by URL from a parent module script.
    if (script_data.source.parent_script_id) {
      NodeScriptLocal* const parent_node = script_tracker_.GetScriptNode(
          isolate, script_data.source.parent_script_id);
      AddEdge<EdgeExecute>(parent_node, code_node, frame_id);
    } else if (script_data.source.dom_node_id != blink::kInvalidDOMNodeId) {
      // If this is a root-level module script, it can still be associated with
      // an HTML script element
      NodeHTMLElement* const script_elm_node =
          GetHTMLElementNode(script_data.source.dom_node_id);
      AddEdge<EdgeExecute>(script_elm_node, code_node, frame_id);
    }
    return;
  }

  if (script_data.source.parent_script_id) {
    const ScriptId parent_script_id = script_data.source.parent_script_id;
    NodeScriptLocal* const parent_node =
        script_tracker_.GetPossibleScriptNode(isolate, parent_script_id);
    if (parent_node != nullptr) {
      AddEdge<EdgeExecute>(parent_node, code_node, frame_id);
    } else if (remote_scripts_.Contains(parent_script_id)) {
      auto remote_script = remote_scripts_.at(parent_script_id);
      AddEdge<EdgeExecute>(remote_script, code_node, frame_id);
    } else {
      NodeScriptRemote* const remote_script =
          AddNode<NodeScriptRemote>(parent_script_id);
      remote_scripts_.insert(parent_script_id, remote_script);
      AddEdge<EdgeExecute>(remote_script, code_node, frame_id);
    }
  } else if (script_data.source.dom_node_id != blink::kInvalidDOMNodeId) {
    NodeHTMLElement* const script_elm_node =
        GetHTMLElementNode(script_data.source.dom_node_id);
    AddEdge<EdgeExecute>(script_elm_node, code_node, frame_id);
  } else {
    NodeActor* const acting_node = GetCurrentActingNode(execution_context);
    AddEdge<EdgeExecute>(acting_node, code_node, frame_id);
  }
}

void PageGraph::RegisterScriptCompilationFromAttr(
    blink::ExecutionContext* execution_context,
    const ScriptId script_id,
    const ScriptData& script_data) {
  FrameId frame_id = GetFrameId(execution_context);
  String attr_name = script_data.source.function_name;
  VLOG(1) << "RegisterScriptCompilationFromAttr) script id: " << script_id
          << ", frame id: " << frame_id
          << ", node id: " << script_data.source.dom_node_id
          << ", attr name: " << attr_name;
  NodeScript* const code_node = script_tracker_.AddScriptNode(
      v8::Isolate::GetCurrent(), script_id, script_data);
  NodeHTMLElement* const html_node =
      GetHTMLElementNode(script_data.source.dom_node_id);
  AddEdge<EdgeExecuteAttr>(html_node, code_node, frame_id, attr_name);
}

// Functions for handling storage read, write, and deletion
void PageGraph::RegisterStorageRead(blink::ExecutionContext* execution_context,
                                    const String& key,
                                    const blink::PageGraphValue& value,
                                    const StorageLocation location) {
  VLOG(1) << "RegisterStorageRead) key: " << key << ", value: " << value
          << ", location: " << StorageLocationToString(location);
  // Capture the call-site offset for reads, as RegisterStorageWrite does for
  // writes. Passing the out-param is what enables it: a non-null pointer doubles
  // as the "include position" flag, because materializing V8 source positions is
  // not free.
  ScriptPosition script_position = 0;
  NodeActor* acting_node =
      GetCurrentActingNode(execution_context, &script_position);

  if (!acting_node->IsNodeScript()) {
    acting_node = GetUnknownActorNode();
  }

  NodeStorage* storage_node = nullptr;
  switch (location) {
    case StorageLocation::kCookie:
      storage_node = cookie_jar_node_;
      break;
    case StorageLocation::kLocalStorage:
      storage_node = local_storage_node_;
      break;
    case StorageLocation::kSessionStorage:
      storage_node = session_storage_node_;
      break;
  }

  FrameId frame_id = GetFrameId(execution_context);
  AddEdge<EdgeStorageReadCall>(acting_node, storage_node, frame_id, key,
                               script_position);
  AddEdge<EdgeStorageReadResult>(storage_node, acting_node, frame_id, key,
                                 value);
}

void PageGraph::RegisterStorageWrite(blink::ExecutionContext* execution_context,
                                     const String& key,
                                     const blink::PageGraphValue& value,
                                     const StorageLocation location,
                                     const CookieSource cookie_source) {
  VLOG(1) << "RegisterStorageWrite) key: " << key << ", value: " << value
          << ", location: " << StorageLocationToString(location)
          << ", cookie source: " << CookieSourceToString(cookie_source);
  ScriptPosition script_position = 0;
  NodeActor* acting_node =
      GetCurrentActingNode(execution_context, &script_position);

  if (!acting_node->IsNodeScript()) {
    acting_node = GetUnknownActorNode();
  }

  NodeStorage* storage_node = nullptr;
  switch (location) {
    case StorageLocation::kCookie:
      storage_node = cookie_jar_node_;
      break;
    case StorageLocation::kLocalStorage:
      storage_node = local_storage_node_;
      break;
    case StorageLocation::kSessionStorage:
      storage_node = session_storage_node_;
      break;
  }

  FrameId frame_id = GetFrameId(execution_context);
  AddEdge<EdgeStorageSet>(acting_node, storage_node, frame_id, key, value,
                          script_position, cookie_source);
}

void PageGraph::RegisterStorageDelete(
    blink::ExecutionContext* execution_context,
    const String& key,
    const StorageLocation location) {
  VLOG(1) << "RegisterStorageDelete) key: " << key
          << ", location: " << StorageLocationToString(location);
  NodeActor* acting_node = GetCurrentActingNode(execution_context);

  if (!acting_node->IsNodeScript()) {
    acting_node = GetUnknownActorNode();
  }

  NodeStorage* storage_node = nullptr;
  switch (location) {
    case StorageLocation::kLocalStorage:
      storage_node = local_storage_node_;
      break;
    case StorageLocation::kSessionStorage:
      storage_node = session_storage_node_;
      break;
    case StorageLocation::kCookie:
      // Reached via cookieStore.delete(). Record the deletion against the cookie
      // jar (document.cookie deletions arrive as expiring StorageSet writes).
      storage_node = cookie_jar_node_;
      break;
  }

  FrameId frame_id = GetFrameId(execution_context);
  AddEdge<EdgeStorageDelete>(acting_node, storage_node, frame_id, key);
}

void PageGraph::RegisterStorageClear(blink::ExecutionContext* execution_context,
                                     const StorageLocation location) {
  VLOG(1) << "RegisterStorageClear) location: "
          << StorageLocationToString(location);
  NodeActor* acting_node = GetCurrentActingNode(execution_context);

  if (!acting_node->IsNodeScript()) {
    acting_node = GetUnknownActorNode();
  }

  NodeStorage* storage_node = nullptr;
  switch (location) {
    case StorageLocation::kLocalStorage:
      storage_node = local_storage_node_;
      break;
    case StorageLocation::kSessionStorage:
      storage_node = session_storage_node_;
      break;
    case StorageLocation::kCookie:
      CHECK(location != StorageLocation::kCookie);
  }

  FrameId frame_id = GetFrameId(execution_context);
  AddEdge<EdgeStorageClear>(acting_node, storage_node, frame_id);
}

void PageGraph::RegisterWebAPICall(blink::ExecutionContext* execution_context,
                                   const MethodName& method,
                                   const blink::PageGraphValues& arguments) {
  FrameId frame_id = GetFrameId(execution_context);
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "RegisterWebAPICall) method: " << method
            << ", frame id: " << frame_id << ", arguments: " << arguments;
  }

  ScriptPosition script_position;
  NodeActor* const acting_node =
      GetCurrentActingNode(execution_context, &script_position);
  if (!IsA<NodeScriptLocal>(acting_node)) {
    // Ignore internal usage.
    return;
  }

  NodeJSWebAPI* js_webapi_node = GetJSWebAPINode(method);
  AddEdge<EdgeJSCall>(static_cast<NodeScriptLocal*>(acting_node),
                      js_webapi_node, frame_id, std::move(arguments),
                      script_position);
}

void PageGraph::RegisterWebAPIResult(blink::ExecutionContext* execution_context,
                                     const MethodName& method,
                                     const blink::PageGraphValue& result) {
  VLOG(2) << "RegisterWebAPIResult) method: " << method
          << ", result: " << result;

  NodeActor* const caller_node = GetCurrentActingNode(execution_context);
  if (!IsA<NodeScript>(caller_node)) {
    // Ignore internal usage.
    return;
  }

  DCHECK(js_webapi_nodes_.Contains(method));
  NodeJSWebAPI* js_webapi_node = GetJSWebAPINode(method);
  FrameId frame_id = GetFrameId(execution_context);
  AddEdge<EdgeJSResult>(js_webapi_node,
                        static_cast<NodeScriptLocal*>(caller_node), frame_id,
                        result);
}

void PageGraph::RegisterJSBuiltInCall(blink::ExecutionContext* receiver_context,
                                      const char* builtin_name,
                                      const blink::PageGraphValues& arguments) {
  FrameId frame_id = GetFrameId(receiver_context);
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "RegisterJSBuiltInCall) built in: " << builtin_name
            << ", frame id: " << frame_id << ", arguments: " << arguments;
  }

  ScriptPosition script_position;
  NodeActor* const acting_node =
      GetCurrentActingNode(receiver_context, &script_position);
  if (!IsA<NodeScript>(acting_node)) {
    // Ignore internal usage.
    return;
  }

  NodeJSBuiltin* js_builtin_node = GetJSBuiltinNode(builtin_name);

  AddEdge<EdgeJSCall>(static_cast<NodeScriptLocal*>(acting_node),
                      js_builtin_node, frame_id, arguments, script_position);
}

void PageGraph::RegisterJSBuiltInResponse(
    blink::ExecutionContext* receiver_context,
    const char* builtin_name,
    const blink::PageGraphValue& result) {
  FrameId frame_id = GetFrameId(receiver_context);
  VLOG(2) << "RegisterJSBuiltInResponse) built in: " << builtin_name
          << ", frame id: " << frame_id << ", result: " << result;

  NodeActor* const caller_node = GetCurrentActingNode(receiver_context);
  if (!IsA<NodeScript>(caller_node)) {
    // Ignore internal usage.
    return;
  }

  DCHECK(js_builtin_nodes_.Contains(builtin_name));
  NodeJSBuiltin* js_builtin_node = GetJSBuiltinNode(builtin_name);
  AddEdge<EdgeJSResult>(js_builtin_node,
                        static_cast<NodeScriptLocal*>(caller_node), frame_id,
                        result);
}

void PageGraph::RegisterBindingEvent(blink::ExecutionContext* execution_context,
                                     const Binding binding,
                                     const BindingType binding_type,
                                     const BindingEvent binding_event) {
  VLOG(2) << "RegisterBindingEvent) binding: " << binding
          << ", event: " << binding_event;

  NodeBinding* binding_node = nullptr;
  NodeBindingEvent* binding_event_node = nullptr;

  for (const auto& executing_script :
       v8::page_graph::GetAllExecutingScripts(v8::Isolate::GetCurrent())) {
    NodeScriptLocal* const script_node = script_tracker_.GetScriptNode(
        v8::Isolate::GetCurrent(), executing_script.script_id);
    const ScriptPosition script_position = executing_script.script_position;
    if (!binding_node) {
      binding_node = GetBindingNode(binding, binding_type);
    }

    if (!binding_event_node) {
      binding_event_node = AddNode<NodeBindingEvent>(binding_event);
      AddEdge<EdgeBinding>(binding_event_node, binding_node);
    }

    AddEdge<EdgeBindingEvent>(script_node, binding_event_node, script_position);
  }
}

NodeActor* PageGraph::GetCurrentActingNode(
    blink::ExecutionContext* execution_context,
    ScriptPosition* out_script_position) {
  const ScriptId current_script_id =
      GetExecutingScriptId(execution_context, out_script_position);

  static ScriptId last_reported_script_id = 0;
  const bool should_log = last_reported_script_id != current_script_id;
  last_reported_script_id = current_script_id;
  if (should_log) {
    VLOG(1) << "GetCurrentActingNode) script id: " << current_script_id;
  }

  if (current_script_id != 0) {
    return script_tracker_.GetScriptNode(v8::Isolate::GetCurrent(),
                                         current_script_id);
  }

  DCHECK(execution_context_nodes_.Contains(execution_context));
  return execution_context_nodes_.at(execution_context).parser_node;
}

ScriptId PageGraph::GetExecutingScriptId(
    blink::ExecutionContext* execution_context,
    ScriptPosition* out_script_position) const {
  auto executing_script = v8::page_graph::GetExecutingScript(
      v8::Isolate::GetCurrent(), out_script_position != nullptr);
  if (out_script_position) {
    *out_script_position = executing_script.script_position;
  }
  return executing_script.script_id;
}

NodeUnknown* PageGraph::GetUnknownActorNode() {
  if (unknown_actor_node_ == nullptr) {
    unknown_actor_node_ = AddNode<NodeUnknown>();
  }
  return unknown_actor_node_;
}

NodeResource* PageGraph::GetResourceNodeForUrl(const KURL& url) {
  auto resource_node_it = resource_nodes_.find(url);
  if (resource_node_it != resource_nodes_.end()) {
    return resource_node_it->value;
  }
  return AddNode<NodeResource>(url);
}

NodeAdFilter* PageGraph::GetAdFilterNodeForRule(const String& rule) {
  auto ad_filter_node_it = ad_filter_nodes_.find(rule);
  if (ad_filter_node_it != ad_filter_nodes_.end()) {
    return ad_filter_node_it->value;
  }
  auto* ad_filter_node = AddNode<NodeAdFilter>(rule);
  AddEdge<EdgeFilter>(ad_shield_node_, ad_filter_node);
  return ad_filter_node;
}

NodeTrackerFilter* PageGraph::GetTrackerFilterNodeForHost(const String& host) {
  auto tracker_filter_it = tracker_filter_nodes_.find(host);
  if (tracker_filter_it != tracker_filter_nodes_.end()) {
    return tracker_filter_it->value;
  }
  auto* filter_node = AddNode<NodeTrackerFilter>(host);
  AddEdge<EdgeFilter>(tracker_shield_node_, filter_node);
  return filter_node;
}

NodeFingerprintingFilter* PageGraph::GetFingerprintingFilterNodeForRule(
    const FingerprintingRule& rule) {
  auto fingerprinting_filter_node_it = fingerprinting_filter_nodes_.find(rule);
  if (fingerprinting_filter_node_it != fingerprinting_filter_nodes_.end()) {
    return fingerprinting_filter_node_it->second;
  }
  auto* filter_node = AddNode<NodeFingerprintingFilter>(rule);
  AddEdge<EdgeFilter>(fingerprinting_shield_node_, filter_node);
  return filter_node;
}

NodeJSWebAPI* PageGraph::GetJSWebAPINode(const MethodName& method) {
  auto js_webapi_node_it = js_webapi_nodes_.find(method);
  if (js_webapi_node_it != js_webapi_nodes_.end()) {
    return js_webapi_node_it->value;
  }
  return AddNode<NodeJSWebAPI>(method);
}

NodeJSBuiltin* PageGraph::GetJSBuiltinNode(const MethodName& method) {
  auto js_builtin_node_it = js_builtin_nodes_.find(method);
  if (js_builtin_node_it != js_builtin_nodes_.end()) {
    return js_builtin_node_it->value;
  }
  return AddNode<NodeJSBuiltin>(method);
}

NodeBinding* PageGraph::GetBindingNode(const Binding binding,
                                       const BindingType binding_type) {
  auto binding_node_it = binding_nodes_.find(binding);
  if (binding_node_it != binding_nodes_.end()) {
    return binding_node_it->value;
  }
  return AddNode<NodeBinding>(binding, binding_type);
}

bool PageGraph::IsRootFrame() const {
  return GetSupplementable()->IsLocalRoot();
}

}  // namespace blink
