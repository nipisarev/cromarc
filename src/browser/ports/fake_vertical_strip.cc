// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/fake_vertical_strip.h"

#include <utility>

#include "base/memory/raw_ref.h"

namespace cromarc {

namespace {

class FakeHold : public VerticalStrip::Hold {
 public:
  explicit FakeHold(int& holds) : holds_(holds) { ++*holds_; }
  ~FakeHold() override { --*holds_; }

 private:
  const raw_ref<int> holds_;
};

}  // namespace

FakeVerticalStrip::FakeVerticalStrip() = default;
FakeVerticalStrip::~FakeVerticalStrip() = default;

void FakeVerticalStrip::UserCollapses(bool collapse) {
  collapsed = collapse;
  on_collapse_.Notify();
}

void FakeVerticalStrip::UserResizes(int width) {
  resizing = true;
  expanded_width = width;
  resizing = false;
  on_resize_ended_.Notify();
}

bool FakeVerticalStrip::IsShown() const {
  return shown;
}

bool FakeVerticalStrip::IsCollapsed() const {
  return collapsed;
}

bool FakeVerticalStrip::IsCollapsedOrCollapsing() const {
  return collapsed || collapsing;
}

void FakeVerticalStrip::RequestCollapse(bool collapse) {
  collapse_requests.push_back(collapse);
  UserCollapses(collapse);
}

int FakeVerticalStrip::ExpandedWidth() const {
  return expanded_width;
}

void FakeVerticalStrip::SetExpandedWidth(int width) {
  expanded_width = width;
}

bool FakeVerticalStrip::IsResizing() const {
  return resizing;
}

base::CallbackListSubscription FakeVerticalStrip::OnCollapseChanged(
    base::RepeatingClosure callback) {
  return on_collapse_.Add(std::move(callback));
}

base::CallbackListSubscription FakeVerticalStrip::OnResizeEnded(
    base::RepeatingClosure callback) {
  return on_resize_ended_.Add(std::move(callback));
}

std::optional<gfx::Rect> FakeVerticalStrip::BoundsInScreen() const {
  return bounds;
}

std::unique_ptr<VerticalStrip::Hold> FakeVerticalStrip::KeepOut() {
  if (!bounds) {
    return nullptr;
  }
  return std::make_unique<FakeHold>(holds);
}

}  // namespace cromarc
