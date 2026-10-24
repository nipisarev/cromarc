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
  editTheme: 'Изменить тему…',
  sleepSpace: 'Усыпить space',
  deleteSpace: 'Удалить space…',
  deleteSpaceTitle: 'Удалить space?',
  deleteSpaceBody: (name: string) =>
      `Удалить «${name}»? Вкладки, история, расширения и данные сайтов этого space пропадут. Это нельзя отменить.`,
  delete: 'Удалить',
  cancel: 'Отмена',
  newSpace: 'Создать space',
  library: 'Библиотека',
  icloudSync: 'Синхронизация через iCloud…',
  icloudTitle: 'iCloud Drive',
  icloudBody: 'Закладки и дерево, история, настройки, список расширений и вкладки этого space лежат в iCloud Drive/Cromarc и попадают на другой Mac. Пароли остаются в менеджере паролей. По одному Mac за раз: прежде чем работать на другом Mac, закройте Cromarc на этом.',
  icloudOn: 'Включена',
  icloudOff: 'Выключена',
  icloudOnAfterRestart: 'Включится после перезапуска',
  icloudOffAfterRestart: 'Выключится после перезапуска',
  icloudEnable: 'Включить',
  icloudDisable: 'Выключить',
  restart: 'Перезапустить',
  today: 'Today',
  rename: 'Переименовать',
  remove: 'Удалить',
  // Zen's Russian strings (zen-general.ftl, zen-vertical-tabs.ftl).
  resetPinned: 'Сбросить закрепленную вкладку',
  resetEssential: 'Сбросить важную вкладку',
  replaceWithCurrent: 'Заменить текущим URL-адресом',
  resetPinButton: 'Сбросить и закрепить вкладку',
  backToPinned: 'Назад к закрепленному адресу',
  separateFromPinned: 'Отделить от закреплённой вкладки',
  unpin: 'Открепить',
  pin: 'Закрепить',
  copyLink: 'Копировать ссылку',
  addToEssentials: 'Добавить в важное',
  removeFromEssentials: 'Удалить из важного',
  // Zen's Essentials promo (zen-essentials-promo-*), shown while a tab is
  // dragged over empty Essentials.
  essentialsPromoLabel: 'Добавить в важное',
  essentialsPromoSublabel: 'Доступ к любимым вкладкам в один клик',
  close: 'Закрыть',
  clear: 'Очистить',
  cleared: 'Today очищен',
  undo: 'Отменить',
  needsAttention: 'Требует внимания',
  unsavedInput: 'не закрыт: несохранённые данные',
  closeAnyway: 'Закрыть всё равно',
  keep: 'Оставить',
  archivesIn: (minutes: number) => `уйдёт в архив через ${minutes} мин`,
};
