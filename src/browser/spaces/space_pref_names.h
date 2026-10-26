// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_SPACES_SPACE_PREF_NAMES_H_
#define CROMARC_BROWSER_SPACES_SPACE_PREF_NAMES_H_

class PrefRegistrySimple;

namespace cromarc {

// Per-space settings live in the space's own profile prefs.
// Emoji or "icon:<name>" shown for the space; "" = none (Zen's no-icon).
inline constexpr char kSpaceIconPref[] = "cromarc.space.icon";
// Position among spaces; -1 = after the ordered ones, by name.
inline constexpr char kSpaceOrderPref[] = "cromarc.space.order";
// Last time the space was the visible one.
inline constexpr char kSpaceLastActivePref[] = "cromarc.space.last_active";
// The theme editor's JSON: {colors: [[r, g, b]...], opacity, texture, scheme}.
inline constexpr char kSpaceThemePref[] = "cromarc.space.theme";

void RegisterSpacePrefs(PrefRegistrySimple* registry);

}  // namespace cromarc

#endif  // CROMARC_BROWSER_SPACES_SPACE_PREF_NAMES_H_
