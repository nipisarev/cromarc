// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import assert from 'node:assert/strict';
import {test} from 'node:test';

import {buildSidebar, cellAt, dropTarget, essentialSlot, essentialsLayout, isAwayFromHome, pageTitleFor} from '../sidebar_model.ts';

function tab(id: string, url: string, extra: Record<string, unknown> = {}) {
  return {
    id, title: `t ${id}`, url, favicon: {dataUrl: ''}, alertStates: [],
    networkState: 0, isActive: false, isSelected: false, isBlocked: false,
    ...extra,
  } as any;
}

function folder(id: string, children: any[], extra: Record<string, unknown> = {}) {
  return {id, title: id, isFolder: true, collapsed: false, essential: false,
    homeUrl: '', tab: null, children, ...extra};
}

function pinned(id: string, homeUrl: string, extra: Record<string, unknown> = {}) {
  return {id, title: id, isFolder: false, collapsed: false, essential: false,
    homeUrl, tab: null, children: [], ...extra};
}

test('tree rows carry depth, parent and index; folders before their children', () => {
  const view = buildSidebar([
    folder('git', [pinned('app', 'https://g/app'), folder('inner', [pinned('x', 'https://x')])]),
    pinned('top', 'https://top'),
  ], []);
  assert.deepEqual(view.rows.map(r => [r.id, r.depth, r.parentId, r.index]), [
    ['git', 0, '', 0], ['app', 1, 'git', 0], ['inner', 1, 'git', 1],
    ['x', 2, 'inner', 0], ['top', 0, '', 1],
  ]);
});

test('a collapsed folder hides its content except the active tab', () => {
  const view = buildSidebar([
    folder('f', [pinned('a', 'https://a', {tab: '1:2'}), pinned('b', 'https://b', {tab: '2:2'})],
        {collapsed: true}),
  ], [tab('1:2', 'https://a'), tab('2:2', 'https://b', {isActive: true})]);
  assert.deepEqual(view.rows.map(r => r.id), ['f', 'b']);
});

test('pinned items are live, asleep or away from home', () => {
  const view = buildSidebar([
    pinned('live', 'https://h/live', {tab: '1:2'}),
    pinned('away', 'https://h/home', {tab: '2:2'}),
    pinned('asleep', 'https://h/z', {tab: '9:2'}),
  ], [tab('1:2', 'https://h/live'), tab('2:2', 'https://h/elsewhere')]);
  const state = Object.fromEntries(view.rows.map(r => [r.id, r.kind === 'pinned' ? r.state : '']));
  assert.deepEqual(state, {live: 'live', away: 'away', asleep: 'asleep'});
});

test('essentials leave the tree rows and form the grid', () => {
  const view = buildSidebar([
    pinned('mail', 'https://mail', {essential: true}),
    folder('f', [pinned('cal', 'https://cal', {essential: true}), pinned('doc', 'https://doc')]),
  ], []);
  assert.deepEqual(view.essentials.map(e => e.id), ['mail', 'cal']);
  assert.deepEqual(view.rows.map(r => r.id), ['f', 'doc']);
});

test('today has the unlinked tabs, newest (last in the strip) first', () => {
  const view = buildSidebar([pinned('p', 'https://p', {tab: '1:2'})],
      [tab('1:2', 'https://p'), tab('2:2', 'https://a'), tab('3:2', 'https://b')]);
  assert.deepEqual(view.today.map(t => t.id), ['3:2', '2:2']);
});

test('deferred tabs move from Today to Needs attention', () => {
  const view = buildSidebar([], [tab('1:2', 'https://a'), tab('2:2', 'https://b')],
      new Set(['1:2']));
  assert.deepEqual(view.attention.map(t => t.id), ['1:2']);
  assert.deepEqual(view.today.map(t => t.id), ['2:2']);
});

test('away from home ignores the fragment and a trailing slash', () => {
  assert.equal(isAwayFromHome('https://h/p', 'https://h/p/'), false);
  assert.equal(isAwayFromHome('https://h/p', 'https://h/p#x'), false);
  assert.equal(isAwayFromHome('https://h/p?a=1', 'https://h/p?a=2'), true);
  assert.equal(isAwayFromHome('https://h/p', 'https://h/q'), true);
});

test('drop targets: before/after a row keep its parent, inside goes into a folder', () => {
  const view = buildSidebar([
    folder('f', [pinned('a', 'https://a'), pinned('b', 'https://b')]),
    pinned('c', 'https://c'),
  ], []);
  const row = (id: string) => view.rows.find(r => r.id === id)!;
  assert.deepEqual(dropTarget(row('b'), 'before'), {parentId: 'f', index: 1});
  assert.deepEqual(dropTarget(row('b'), 'after'), {parentId: 'f', index: 2});
  assert.deepEqual(dropTarget(row('f'), 'inside'), {parentId: 'f', index: 0});
  assert.deepEqual(dropTarget(row('c'), 'after'), {parentId: '', index: 2});
});

test('essentials layouts follow Zen by tile count', () => {
  assert.deepEqual(
      [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12].map(essentialsLayout),
      [0, 2, 0, 2, 3, 1, 0, 0, 1, 0, 0, 0]);
});

test('an essential slot is before the essential there, or after the last', () => {
  const view = buildSidebar([
    pinned('mail', 'https://mail', {essential: true}),
    pinned('doc', 'https://doc'),
    folder('f', [pinned('x', 'https://x'), pinned('cal', 'https://cal', {essential: true})]),
  ], []);
  const essentials = view.essentials;
  assert.deepEqual(essentialSlot(essentials, 0, 3), {parentId: '', index: 0});
  assert.deepEqual(essentialSlot(essentials, 1, 3), {parentId: 'f', index: 1});
  assert.deepEqual(essentialSlot(essentials, 2, 3), {parentId: 'f', index: 2});
  assert.deepEqual(essentialSlot([], 0, 3), {parentId: '', index: 3});
});

test('the cell under a point, kept inside the grid', () => {
  // Two columns of 50×46 with a 4px gap; the second row has one cell.
  const cells = [
    {left: 0, top: 0, width: 50, height: 46},
    {left: 54, top: 0, width: 50, height: 46},
    {left: 0, top: 50, width: 50, height: 46},
  ];
  assert.equal(cellAt(cells, 10, 10), 0);
  assert.equal(cellAt(cells, 60, 10), 1);
  assert.equal(cellAt(cells, 10, 60), 2);
  assert.equal(cellAt(cells, 52, 10), -1);
  assert.equal(cellAt(cells, -30, -30), 0);
  assert.equal(cellAt(cells, 500, 10), 1);
  assert.equal(cellAt(cells, 10, 500), 2);
  assert.equal(cellAt(cells, 80, 80), -1);
  assert.equal(cellAt([], 0, 0), -1);
});

test('an untitled pinned item shows its host, then takes the page title', () => {
  const at = (title: string, url: string) => buildSidebar(
      [pinned('mail', 'https://www.mail.example/inbox', {title: '', tab: 'm'})],
      [tab('m', url, {title})]).rows[0] as any;
  const loading = at('www.mail.example/inbox', 'https://www.mail.example/inbox');
  assert.equal(loading.title, 'mail.example');
  assert.equal(loading.untitled, true);
  assert.equal(pageTitleFor(loading, true), '');
  const loaded = at('Inbox', 'https://www.mail.example/inbox');
  assert.equal(pageTitleFor(loaded, true), 'Inbox');
  assert.equal(pageTitleFor(loaded, false), '');
  assert.equal(pageTitleFor(at('Elsewhere', 'https://other.example/'), true), '');
  const titled = buildSidebar([pinned('doc', 'https://doc', {title: 'Doc', tab: 'd'})],
      [tab('d', 'https://doc', {title: 'Page'})]).rows[0] as any;
  assert.equal(titled.untitled, false);
  assert.equal(pageTitleFor(titled, true), '');
});
