/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/request/edge_request_complete.h"

#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/node_resource.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graphml.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace brave_page_graph {

EdgeRequestComplete::EdgeRequestComplete(GraphItemContext* context,
                                         NodeResource* out_node,
                                         GraphNode* in_node,
                                         const InspectorId request_id,
                                         const FrameId& frame_id,
                                         const blink::String& resource_type,
                                         const blink::String& hash)
    : EdgeRequestResponse(context,
                          out_node,
                          in_node,
                          request_id,
                          frame_id,
                          kRequestStatusComplete),
      resource_type_(resource_type),
      hash_(hash) {}

EdgeRequestComplete::~EdgeRequestComplete() = default;

ItemName EdgeRequestComplete::GetItemName() const {
  return "request complete";
}

ItemDesc EdgeRequestComplete::GetItemDesc() const {
  return blink::StrCat(
      {EdgeRequestResponse::GetItemDesc(), " [", resource_type_, "]"});
}

void EdgeRequestComplete::AddGraphMLAttributes(xmlDocPtr doc,
                                               xmlNodePtr parent_node) const {
  EdgeRequestResponse::AddGraphMLAttributes(doc, parent_node);
  // SHA-256 of the response body, accumulated in TrackedRequest as the bytes
  // arrive. It has always been computed and handed to this edge, but never
  // emitted, so the attribute was declared with no producer. It identifies a
  // body without recording its content — which is what makes it a usable join
  // key for the crawler's body sidecar.
  if (!hash_.empty()) {
    GraphMLAttrDefForType(kGraphMLAttrDefResponseHash)
        ->AddValueNode(doc, parent_node, hash_);
  }
}

bool EdgeRequestComplete::IsEdgeRequestComplete() const {
  return true;
}

}  // namespace brave_page_graph
