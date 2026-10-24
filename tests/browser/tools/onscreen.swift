// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

import CoreGraphics
import Foundation
// Prints "title|x,y,w,h" for every on-screen normal window of a process.
let pid = Int32(CommandLine.arguments[1])!
let infos = CGWindowListCopyWindowInfo([.optionOnScreenOnly], kCGNullWindowID) as! [[String: Any]]
for w in infos where (w[kCGWindowOwnerPID as String] as? Int32) == pid && (w[kCGWindowLayer as String] as? Int) == 0 {
  let b = w[kCGWindowBounds as String] as! [String: Any]
  print("\(w[kCGWindowNumber as String]!)|\(w[kCGWindowName as String] as? String ?? "")|\(b["X"]!),\(b["Y"]!),\(b["Width"]!),\(b["Height"]!)")
}
