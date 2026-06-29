/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/edge/storage/edge_storage_set.h"

#include "base/check.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/actor/node_actor.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graph_item/node/storage/node_storage.h"
#include "brave/third_party/blink/renderer/core/brave_page_graph/graphml.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace brave_page_graph {

EdgeStorageSet::EdgeStorageSet(GraphItemContext* context,
                               NodeActor* out_node,
                               NodeStorage* in_node,
                               const FrameId& frame_id,
                               const blink::String& key,
                               const blink::PageGraphValue& value,
                               const int script_position,
                               const CookieSource cookie_source)
    : EdgeStorage(context, out_node, in_node, frame_id, key),
      value_(blink::PageGraphValueToString(value)),
      script_position_(script_position),
      cookie_source_(cookie_source) {
  CHECK(!out_node->IsNodeParser());
}

EdgeStorageSet::~EdgeStorageSet() = default;

ItemName EdgeStorageSet::GetItemName() const {
  return "storage set";
}

ItemDesc EdgeStorageSet::GetItemDesc() const {
  return blink::StrCat(
      {EdgeStorage::GetItemDesc(), " [value: ", blink::String(value_), "]"});
}

void EdgeStorageSet::AddGraphMLAttributes(xmlDocPtr doc,
                                          xmlNodePtr parent_node) const {
  EdgeStorage::AddGraphMLAttributes(doc, parent_node);
  GraphMLAttrDefForType(kGraphMLAttrDefValue)
      ->AddValueNode(doc, parent_node, value_);
  // Offset of the assigning statement within the acting script source, matching
  // the "script position" recorded on js call edges (see EdgeJSCall).
  GraphMLAttrDefForType(kGraphMLAttrDefScriptPosition)
      ->AddValueNode(doc, parent_node, script_position_);
  // Only cookie writes carry a source channel; other storage locations leave
  // this unset (kUnknown).
  if (cookie_source_ != CookieSource::kUnknown) {
    GraphMLAttrDefForType(kGraphMLAttrDefCookieSource)
        ->AddValueNode(doc, parent_node, CookieSourceToString(cookie_source_));
  }
}

bool EdgeStorageSet::IsEdgeStorageSet() const {
  return true;
}

}  // namespace brave_page_graph
