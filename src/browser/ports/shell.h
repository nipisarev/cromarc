// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef CROMARC_BROWSER_PORTS_SHELL_H_
#define CROMARC_BROWSER_PORTS_SHELL_H_

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace cromarc {

// The system's file manager and default apps.
class Shell {
 public:
  virtual ~Shell() = default;

  // Opens the file in its default app.
  virtual void OpenFile(Profile* profile, const base::FilePath& path) = 0;
  // Shows it selected in Finder.
  virtual void ShowInFolder(Profile* profile, const base::FilePath& path) = 0;
};

}  // namespace cromarc

#endif  // CROMARC_BROWSER_PORTS_SHELL_H_
