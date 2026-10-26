// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ports/tree.h"

namespace cromarc {

Tree::Node::Node() = default;
Tree::Node::Node(const Node&) = default;
Tree::Node::Node(Node&&) = default;
Tree::Node& Tree::Node::operator=(const Node&) = default;
Tree::Node& Tree::Node::operator=(Node&&) = default;
Tree::Node::~Node() = default;

}  // namespace cromarc
