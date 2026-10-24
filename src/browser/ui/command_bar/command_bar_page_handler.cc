// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/command_bar/command_bar_page_handler.h"

#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "content/public/browser/web_contents.h"
#include "cromarc/browser/archive/archive_service.h"
#include "cromarc/browser/tree/tree.h"
#include "cromarc/browser/tree/tree_service.h"
#include "cromarc/browser/ui/command_bar/command_bar.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/window_open_disposition.h"

namespace cromarc {

namespace {

using command_bar::mojom::Result;
using command_bar::mojom::ResultKind;
using command_bar::mojom::ResultPtr;

constexpr size_t kMaxTabs = 4;
constexpr size_t kMaxPinned = 4;
constexpr size_t kMaxSuggestions = 6;
constexpr size_t kMaxArchive = 4;

// Every word of the query, lowercased.
std::vector<std::u16string> Words(const std::u16string& text) {
  std::vector<std::u16string> words;
  for (std::u16string_view word : base::SplitStringPiece(
           text, base::kWhitespaceUTF16, base::TRIM_WHITESPACE,
           base::SPLIT_WANT_NONEMPTY)) {
    words.push_back(base::i18n::ToLower(word));
  }
  return words;
}

bool Matches(const std::vector<std::u16string>& words,
             const std::u16string& title,
             const GURL& url) {
  const std::u16string haystack =
      base::i18n::ToLower(title + u" " + base::UTF8ToUTF16(url.spec()));
  for (const std::u16string& word : words) {
    if (haystack.find(word) == std::u16string::npos) {
      return false;
    }
  }
  return true;
}

ResultPtr MakeResult(ResultKind kind,
                     std::string id,
                     const std::u16string& title,
                     const std::u16string& subtitle,
                     const GURL& url) {
  auto result = Result::New();
  result->kind = kind;
  result->id = std::move(id);
  result->title = base::UTF16ToUTF8(title);
  result->subtitle = base::UTF16ToUTF8(subtitle);
  result->url = url;
  return result;
}

}  // namespace

CommandBarPageHandler::CommandBarPageHandler(
    mojo::PendingReceiver<command_bar::mojom::PageHandler> receiver,
    mojo::PendingRemote<command_bar::mojom::Page> page,
    BrowserWindowInterface* browser,
    TreeService* tree_service,
    ArchiveService* archive_service)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(browser),
      tree_service_(tree_service),
      archive_service_(archive_service) {
  Profile* profile = browser_->GetProfile();
  autocomplete_ = std::make_unique<AutocompleteController>(
      std::make_unique<ChromeAutocompleteProviderClient>(profile),
      AutocompleteControllerConfig{
          .provider_types = AutocompleteClassifier::DefaultOmniboxProviders()});
  autocomplete_->AddObserver(this);
  if (CommandBar* bar = CommandBar::Get(browser_)) {
    bar->SetPageHandler(this);
    NotifyShown(bar->InitialText());
  }
}

CommandBarPageHandler::~CommandBarPageHandler() {
  autocomplete_->RemoveObserver(this);
  if (CommandBar* bar = CommandBar::Get(browser_)) {
    bar->SetPageHandler(nullptr);
  }
}

void CommandBarPageHandler::NotifyShown(const std::u16string& text) {
  page_->OnShown(base::UTF16ToUTF8(text));
  Query(base::UTF16ToUTF8(text));
}

void CommandBarPageHandler::Query(const std::string& text_utf8) {
  text_ = base::UTF8ToUTF16(text_utf8);
  const std::vector<std::u16string> words = Words(text_);

  // Tabs of this window; with no text, the most recent ones.
  tabs_.clear();
  tab_ids_.clear();
  TabStripModel* model = browser_->GetTabStripModel();
  for (int i = model->count() - 1; i >= 0 && tabs_.size() < kMaxTabs; --i) {
    tabs::TabInterface* tab = model->GetTabAtIndex(i);
    content::WebContents* contents = tab->GetContents();
    const GURL url = contents->GetLastCommittedURL();
    if (!Matches(words, contents->GetTitle(), url)) {
      continue;
    }
    const std::string id =
        base::NumberToString(tab->GetHandle().raw_value());
    tab_ids_.emplace(id, tab->GetHandle());
    tabs_.push_back(MakeResult(ResultKind::kTab, id, contents->GetTitle(),
                               base::UTF8ToUTF16(url.spec()), url));
  }

  // Pinned items of the space.
  pinned_.clear();
  if (!words.empty() && tree_service_->loaded()) {
    auto visit = [&](const bookmarks::BookmarkNode* node, auto& self) -> void {
      for (const auto& child : node->children()) {
        if (pinned_.size() >= kMaxPinned) {
          return;
        }
        if (child->is_folder()) {
          self(child.get(), self);
        } else if (Matches(words, child->GetTitle(), child->url())) {
          pinned_.push_back(MakeResult(
              ResultKind::kPinned, Tree::IdOf(child.get()), child->GetTitle(),
              base::UTF8ToUTF16(child->url().spec()), child->url()));
        }
      }
    };
    visit(tree_service_->tree().Root(), visit);
  }

  // The archive, asynchronously.
  archive_.clear();
  if (!words.empty()) {
    archive_service_->Search(
        text_, kMaxArchive,
        base::BindOnce(&CommandBarPageHandler::OnArchiveResults,
                       weak_factory_.GetWeakPtr(), text_));
  }

  // The omnibox, asynchronously (results arrive in OnResultChanged).
  if (words.empty()) {
    autocomplete_->Stop(AutocompleteStopReason::kClobbered);
  } else {
    AutocompleteInput input(
        text_, metrics::OmniboxEventProto::OTHER,
        ChromeAutocompleteSchemeClassifier(browser_->GetProfile()));
    autocomplete_->Start(input);
  }
  Publish();
}

void CommandBarPageHandler::OnArchiveResults(
    std::u16string text,
    std::vector<ArchiveEntry> entries) {
  if (text != text_) {
    return;
  }
  archive_ = std::move(entries);
  Publish();
}

void CommandBarPageHandler::OnResultChanged(AutocompleteController* controller,
                                            bool default_match_changed) {
  Publish();
}

void CommandBarPageHandler::Publish() {
  std::vector<ResultPtr> results;
  std::set<GURL> seen;
  auto add = [&](ResultPtr result) {
    if (seen.insert(result->url).second) {
      results.push_back(std::move(result));
    }
  };
  for (const ResultPtr& r : tabs_) {
    add(r.Clone());
  }
  for (const ResultPtr& r : pinned_) {
    add(r.Clone());
  }
  if (!text_.empty()) {
    size_t count = 0;
    const AutocompleteResult& matches = autocomplete_->result();
    for (size_t i = 0; i < matches.size() && count < kMaxSuggestions; ++i) {
      const AutocompleteMatch& match = matches.match_at(i);
      if (!match.destination_url.is_valid()) {
        continue;
      }
      add(MakeResult(ResultKind::kSuggestion, base::NumberToString(i),
                     match.contents, match.description,
                     match.destination_url));
      ++count;
    }
  }
  for (size_t i = 0; i < archive_.size(); ++i) {
    const ArchiveEntry& entry = archive_[i];
    add(MakeResult(ResultKind::kArchive, base::NumberToString(entry.id),
                   entry.title, base::UTF8ToUTF16(entry.url.spec()),
                   entry.url));
  }
  page_->OnResults(base::UTF16ToUTF8(text_), std::move(results));
}

void CommandBarPageHandler::Open(ResultPtr result) {
  CommandBar* bar = CommandBar::Get(browser_);
  const bool edit_current_url = bar && bar->edit_current_url();
  if (bar) {
    bar->Hide();
  }

  switch (result->kind) {
    case ResultKind::kTab: {
      auto it = tab_ids_.find(result->id);
      tabs::TabInterface* tab = it == tab_ids_.end() ? nullptr : it->second.Get();
      if (tab) {
        TabStripModel* model = browser_->GetTabStripModel();
        model->ActivateTabAt(model->GetIndexOfTab(tab));
      }
      return;
    }
    case ResultKind::kPinned:
      tree_service_->Open(browser_, result->id);
      return;
    case ResultKind::kSuggestion:
    case ResultKind::kArchive:
      // Already pinned: focus it rather than opening a duplicate (design,
      // component 6).
      if (std::optional<std::string> item =
              tree_service_->FindItemByHomeUrl(result->url)) {
        tree_service_->Open(browser_, *item);
        return;
      }
      browser_->OpenGURL(result->url,
                         edit_current_url
                             ? WindowOpenDisposition::CURRENT_TAB
                             : WindowOpenDisposition::NEW_FOREGROUND_TAB);
      return;
  }
}

void CommandBarPageHandler::Close() {
  if (CommandBar* bar = CommandBar::Get(browser_)) {
    bar->Hide();
  }
}

}  // namespace cromarc
