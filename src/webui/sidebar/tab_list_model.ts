// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import type {Container, Data, Tab} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';
import type {OnCollectionCreatedEvent, OnNodeMovedEvent, OnNodesClosedEvent, OnTabsCreatedEvent, TabChange} from '/tab_strip_api/tab_strip_api_events.mojom-webui.js';
import type {NodeId, Position} from '/tab_strip_api/tab_strip_api_types.mojom-webui.js';

interface Node {
  id: NodeId;
  tab?: Tab;
  children: Node[];
  parent?: Node;
}

function dataId(data: Data): NodeId {
  const variant = Object.values(data).find(value => value !== undefined) as
      {id: NodeId} | undefined;
  if (!variant) {
    throw new Error('TabStripService sent an empty Data union');
  }
  return variant.id;
}

// The browser window's tab strip as TabStripService reports it: a tree of
// pinned/unpinned collections, groups and split tabs. The sidebar shows its
// tabs in order. Every on*() returns false when the event refers to a node
// this model does not have: the caller then re-reads the snapshot.
export class TabListModel {
  private readonly nodes_ = new Map<NodeId, Node>();

  constructor(snapshot: Container) {
    this.adopt_(snapshot);
  }

  tabs(): Tab[] {
    const result: Tab[] = [];
    const visit = (node: Node) => {
      if (node.tab) {
        result.push(node.tab);
      }
      node.children.forEach(visit);
    };
    const roots = [...this.nodes_.values()].filter(node => !node.parent);
    roots.forEach(visit);
    return result;
  }

  onTabsCreated(event: OnTabsCreatedEvent): boolean {
    return event.tabs.every(
        created => this.insert_(
            {id: created.tab.id, tab: created.tab, children: []},
            created.position));
  }

  onNodesClosed(event: OnNodesClosedEvent): boolean {
    return event.nodeIds.every(id => {
      const node = this.nodes_.get(id);
      if (!node) {
        return false;
      }
      this.detach_(node);
      this.forget_(node);
      return true;
    });
  }

  onNodeMoved(event: OnNodeMovedEvent): boolean {
    const node = this.nodes_.get(event.id);
    if (!node) {
      return false;
    }
    this.detach_(node);
    return this.insert_(node, event.to);
  }

  onCollectionCreated(event: OnCollectionCreatedEvent): boolean {
    const node = this.adopt_(event.collection);
    this.detach_(node);
    return this.insert_(node, event.position);
  }

  onTabChanged(change: TabChange): boolean {
    const node = this.nodes_.get(change.data.id);
    if (!node || !node.tab) {
      return false;
    }
    const updated: Tab = {...node.tab};
    for (const [field, changed] of Object.entries(change.mask)) {
      if (changed && field in change.data) {
        const key = field as keyof Tab;
        (updated as Record<keyof Tab, unknown>)[key] = change.data[key];
      }
    }
    node.tab = updated;
    return true;
  }

  private adopt_(container: Container, parent?: Node): Node {
    const node: Node = {
      id: dataId(container.data),
      tab: container.data.tab,
      children: [],
      parent,
    };
    this.nodes_.set(node.id, node);
    node.children = container.children.map(child => this.adopt_(child, node));
    return node;
  }

  private insert_(node: Node, position: Position): boolean {
    const parentId = position.path.components.at(-1);
    const parent = parentId === undefined ? undefined : this.nodes_.get(parentId);
    if (!parent) {
      return false;
    }
    const index = Math.min(position.index, parent.children.length);
    parent.children.splice(index, 0, node);
    node.parent = parent;
    this.nodes_.set(node.id, node);
    return true;
  }

  private detach_(node: Node) {
    if (!node.parent) {
      return;
    }
    const siblings = node.parent.children;
    siblings.splice(siblings.indexOf(node), 1);
    node.parent = undefined;
  }

  private forget_(node: Node) {
    this.nodes_.delete(node.id);
    node.children.forEach(child => this.forget_(child));
  }
}
