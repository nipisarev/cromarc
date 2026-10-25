// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Run: node --test src/webui/sidebar/tests/

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {TabListModel} from '../tab_list_model.ts';

function tab(id: string, extra: Record<string, unknown> = {}) {
  return {
    id,
    title: `title ${id}`,
    url: `https://example.com/${id}`,
    favicon: {dataUrl: ''},
    alertStates: [],
    networkState: 0,
    isActive: false,
    isSelected: false,
    isBlocked: false,
    ...extra,
  };
}

function leaf(id: string, extra: Record<string, unknown> = {}) {
  return {data: {tab: tab(id, extra)}, children: []};
}

// tab strip → [pinned: [p1], unpinned: [a, group g: [b, c], d]]
function snapshot() {
  return {
    data: {tabStrip: {id: 'strip:3'}},
    children: [
      {data: {pinnedTabs: {id: 'pinned:3'}}, children: [leaf('p1:2')]},
      {
        data: {unpinnedTabs: {id: 'unpinned:3'}},
        children: [
          leaf('a:2', {isActive: true}),
          {
            data: {tabGroup: {id: 'g:3'}},
            children: [leaf('b:2'), leaf('c:2')],
          },
          leaf('d:2'),
        ],
      },
    ],
  };
}

function at(parents: string[], index: number) {
  return {path: {components: parents}, index};
}

function ids(model: TabListModel) {
  return model.tabs().map(t => t.id);
}

test('pinned tabs: those in Chrome\'s pinned collection, new ones too', () => {
  const model = new TabListModel(snapshot() as any);
  assert.deepEqual([...model.pinnedTabIds()], ['p1:2']);
  model.onTabsCreated({tabs: [{tab: tab('p2:2'), position: at(['pinned:3'], 1)}]} as any);
  model.onTabsCreated({tabs: [{tab: tab('e:2'), position: at(['unpinned:3'], 0)}]} as any);
  assert.deepEqual([...model.pinnedTabIds()], ['p1:2', 'p2:2']);
});

test('snapshot: tabs in strip order, groups flattened', () => {
  const model = new TabListModel(snapshot() as any);
  assert.deepEqual(ids(model), ['p1:2', 'a:2', 'b:2', 'c:2', 'd:2']);
});

test('created tab lands at its position', () => {
  const model = new TabListModel(snapshot() as any);
  assert.ok(model.onTabsCreated({
    tabs: [{tab: tab('e:2'), position: at(['strip:3', 'unpinned:3'], 1)}],
  } as any));
  assert.ok(model.onTabsCreated({
    tabs: [{tab: tab('f:2'), position: at(['strip:3', 'unpinned:3', 'g:3'], 2)}],
  } as any));
  assert.deepEqual(ids(model), ['p1:2', 'a:2', 'e:2', 'b:2', 'c:2', 'f:2', 'd:2']);
});

test('closed nodes disappear, a closed group takes its tabs', () => {
  const model = new TabListModel(snapshot() as any);
  assert.ok(model.onNodesClosed({nodeIds: ['a:2', 'g:3']} as any));
  assert.deepEqual(ids(model), ['p1:2', 'd:2']);
});

test('moved node changes place, also across parents', () => {
  const model = new TabListModel(snapshot() as any);
  assert.ok(model.onNodeMoved({
    id: 'd:2',
    from: at(['strip:3', 'unpinned:3'], 2),
    to: at(['strip:3', 'unpinned:3'], 0),
  } as any));
  assert.ok(model.onNodeMoved({
    id: 'b:2',
    from: at(['strip:3', 'unpinned:3', 'g:3'], 0),
    to: at(['strip:3', 'pinned:3'], 1),
  } as any));
  assert.deepEqual(ids(model), ['p1:2', 'b:2', 'd:2', 'a:2', 'c:2']);
});

test('created collection is inserted with its tabs', () => {
  const model = new TabListModel(snapshot() as any);
  assert.ok(model.onCollectionCreated({
    position: at(['strip:3', 'unpinned:3'], 2),
    collection: {data: {tabGroup: {id: 'h:3'}}, children: [leaf('x:2')]},
  } as any));
  assert.deepEqual(ids(model), ['p1:2', 'a:2', 'b:2', 'c:2', 'x:2', 'd:2']);
});

test('tab change applies only masked fields', () => {
  const model = new TabListModel(snapshot() as any);
  const mask = {title: true, url: false, favicon: false, alertStates: false,
    networkState: false, isActive: false, isSelected: false, isBlocked: false,
    lastActive: false};
  assert.ok(model.onTabChanged(
      {data: tab('b:2', {title: 'new', url: 'https://other'}), mask} as any));
  const b = model.tabs().find(t => t.id === 'b:2')!;
  assert.equal(b.title, 'new');
  assert.equal(b.url, 'https://example.com/b:2');
});

test('activation marks exactly one tab active', () => {
  const model = new TabListModel(snapshot() as any);
  const mask = {title: false, url: false, favicon: false, alertStates: false,
    networkState: false, isActive: true, isSelected: false, isBlocked: false,
    lastActive: false};
  model.onTabChanged({data: tab('c:2', {isActive: true}), mask} as any);
  model.onTabChanged({data: tab('a:2', {isActive: false}), mask} as any);
  assert.deepEqual(model.tabs().filter(t => t.isActive).map(t => t.id), ['c:2']);
});

test('events about unknown nodes report a desync', () => {
  const model = new TabListModel(snapshot() as any);
  assert.equal(model.onNodesClosed({nodeIds: ['nope:2']} as any), false);
  assert.equal(model.onTabsCreated({
    tabs: [{tab: tab('e:2'), position: at(['strip:3', 'nope:3'], 0)}],
  } as any), false);
  assert.equal(model.onNodeMoved({
    id: 'nope:2', from: at(['strip:3'], 0), to: at(['strip:3'], 0),
  } as any), false);
});
