// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import type {TreeNode} from './sidebar.mojom-webui.js';
import type {Tab} from '/tab_strip_api/tab_strip_api_data_model.mojom-webui.js';

// asleep: no live tab, opening loads the home URL. away: the live tab left the
// home URL (the row shows ↩).
export type PinnedState = 'live'|'asleep'|'away';

interface RowBase {
  id: string;
  title: string;
  depth: number;
  // Position in the tree: parent folder ("" = root) and index among its
  // children, as Tree::Move expects them.
  parentId: string;
  index: number;
}

export interface FolderRow extends RowBase {
  kind: 'folder';
  collapsed: boolean;
}

export interface PinnedRow extends RowBase {
  kind: 'pinned';
  homeUrl: string;
  essential: boolean;
  state: PinnedState;
  tab: Tab|null;
}

export type TreeRow = FolderRow|PinnedRow;

export interface SidebarView {
  essentials: PinnedRow[];
  rows: TreeRow[];
  today: Tab[];
}

function normalize(url: string): string {
  try {
    const u = new URL(url);
    u.hash = '';
    return u.href.endsWith('/') ? u.href.slice(0, -1) : u.href;
  } catch {
    return url;
  }
}

export function isAwayFromHome(homeUrl: string, currentUrl: string): boolean {
  return normalize(homeUrl) !== normalize(currentUrl);
}

export function buildSidebar(tree: TreeNode[], tabs: Tab[]): SidebarView {
  const tabsById = new Map(tabs.map(t => [t.id, t]));
  const linked = new Set<string>();
  const essentials: PinnedRow[] = [];
  const rows: TreeRow[] = [];

  const pinnedRow = (node: TreeNode, depth: number, parentId: string,
                     index: number): PinnedRow => {
    const tab = node.tab ? tabsById.get(node.tab) ?? null : null;
    if (tab) {
      linked.add(tab.id);
    }
    const state: PinnedState =
        !tab ? 'asleep' : isAwayFromHome(node.homeUrl, tab.url) ? 'away' : 'live';
    return {
      kind: 'pinned', id: node.id, title: node.title, depth, parentId, index,
      homeUrl: node.homeUrl, essential: node.essential, state, tab,
    };
  };

  // `hidden`: inside a collapsed folder. Only the active tab stays visible
  // there, as in Zen.
  const visit = (nodes: TreeNode[], depth: number, parentId: string,
                 hidden: boolean): void => {
    nodes.forEach((node, index) => {
      if (node.isFolder) {
        if (!hidden) {
          rows.push({
            kind: 'folder', id: node.id, title: node.title, depth, parentId,
            index, collapsed: node.collapsed,
          });
        }
        visit(node.children, depth + 1, node.id, hidden || node.collapsed);
        return;
      }
      const row = pinnedRow(node, depth, parentId, index);
      if (node.essential) {
        essentials.push(row);
      } else if (!hidden || row.tab?.isActive) {
        rows.push(row);
      }
    });
  };
  visit(tree, 0, '', false);

  const today = tabs.filter(t => !linked.has(t.id)).reverse();
  return {essentials, rows, today};
}

export type DropPosition = 'before'|'after'|'inside';

export function dropTarget(row: TreeRow, position: DropPosition):
    {parentId: string, index: number} {
  if (position === 'inside' && row.kind === 'folder') {
    return {parentId: row.id, index: 0};
  }
  return {
    parentId: row.parentId,
    index: position === 'after' ? row.index + 1 : row.index,
  };
}
