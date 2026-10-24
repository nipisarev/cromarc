// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import CoreGraphics
import Foundation
// Prints the titles of a process's normal windows.
let pid = Int32(CommandLine.arguments[1])!
let infos = CGWindowListCopyWindowInfo([.optionAll], kCGNullWindowID) as! [[String: Any]]
for w in infos where (w[kCGWindowOwnerPID as String] as? Int32) == pid {
  if let name = w[kCGWindowName as String] as? String, !name.isEmpty,
     (w[kCGWindowLayer as String] as? Int) == 0 {
    print(name)
  }
}
