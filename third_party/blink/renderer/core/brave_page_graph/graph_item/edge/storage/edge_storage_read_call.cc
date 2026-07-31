/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/storage/edge_storage_read_call.h"

#include <string>

#include "base/check.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/actor/node_actor.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/storage/node_storage.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graphml.h"

namespace brave_page_graph {

EdgeStorageReadCall::EdgeStorageReadCall(GraphItemContext* context,
                                         NodeActor* out_node,
                                         NodeStorage* in_node,
                                         const FrameId& frame_id,
                                         const blink::String& key,
                                         const int script_position)
    : EdgeStorage(context, out_node, in_node, frame_id, key),
      script_position_(script_position) {
  CHECK(!out_node->IsNodeParser());
}

EdgeStorageReadCall::~EdgeStorageReadCall() = default;

ItemName EdgeStorageReadCall::GetItemName() const {
  return "read storage call";
}

void EdgeStorageReadCall::AddGraphMLAttributes(xmlDocPtr doc,
                                               xmlNodePtr parent_node) const {
  EdgeStorage::AddGraphMLAttributes(doc, parent_node);
  // Offset of the reading statement within the acting script source, matching
  // the "script position" recorded on storage set and js call edges.
  GraphMLAttrDefForType(kGraphMLAttrDefScriptPosition)
      ->AddValueNode(doc, parent_node, script_position_);
}

bool EdgeStorageReadCall::IsEdgeStorageReadCall() const {
  return true;
}

}  // namespace brave_page_graph
