// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// UI strings of the sidebar, in one place for a later move to loadTimeData.
export const strings = {
  newTab: 'Новая вкладка',
  newFolder: 'Новая папка',
  importFromArc: 'Перенести из Arc…',
  hideSidebar: 'Скрыть сайдбар  ⌘S',
  // Zen's space menu (zen-workspaces-panel-*).
  editSpace: 'Изменить space…',
  changeIcon: 'Сменить иконку…',
  sleepSpace: 'Усыпить space',
  deleteSpace: 'Удалить space…',
  deleteSpaceTitle: 'Удалить space?',
  deleteSpaceBody: (name: string) =>
      `Удалить «${name}»? Вкладки, история, расширения и данные сайтов этого space пропадут. Это нельзя отменить.`,
  delete: 'Удалить',
  cancel: 'Отмена',
  newSpace: 'Создать space',
  library: 'Библиотека',
  today: 'Today',
  rename: 'Переименовать',
  remove: 'Удалить',
  reset: 'Вернуть на домашнюю',
  makeHome: 'Сделать текущую страницу домашней',
  unpin: 'Открепить',
  pin: 'Закрепить',
  copyLink: 'Копировать ссылку',
  addToEssentials: 'В Essentials',
  removeFromEssentials: 'Убрать из Essentials',
  close: 'Закрыть',
  backToHome: 'Вернуть на домашнюю страницу',
  clear: 'Очистить',
  cleared: 'Today очищен',
  undo: 'Отменить',
  needsAttention: 'Требует внимания',
  unsavedInput: 'не закрыт: несохранённые данные',
  closeAnyway: 'Закрыть всё равно',
  keep: 'Оставить',
  archivesIn: (minutes: number) => `уйдёт в архив через ${minutes} мин`,
};
