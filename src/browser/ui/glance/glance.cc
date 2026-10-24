// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/glance/glance.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"  // nogncheck: as above
#include "chrome/browser/ui/tabs/tab_strip_model.h"  // nogncheck: as above
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "cromarc/browser/ui/glance/hovered_link.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/base_window.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/page_transition_types.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_delegate.h"

namespace cromarc {

namespace {

// Zen's zen.glance.animation-duration.
constexpr base::TimeDelta kOpenDuration = base::Milliseconds(350);
constexpr base::TimeDelta kCloseDuration = base::Milliseconds(200);
// Animations need frames; a hidden or occluded window gets none, so the end
// state is set by a timer too (as Zen's workspace switch does, issue 9334).
constexpr base::TimeDelta kSafetyMargin = base::Milliseconds(100);
// The card takes 85% of the page area.
constexpr float kCardInset = 0.075f;
constexpr float kRadius = 12.f;
constexpr int kIconSize = 16;
constexpr SkColor kBackdrop = SkColorSetA(SK_ColorBLACK, 0x59);

std::map<BrowserWindowInterface*, std::unique_ptr<Glance>>& Glances() {
  static base::NoDestructor<
      std::map<BrowserWindowInterface*, std::unique_ptr<Glance>>>
      glances;
  return *glances;
}

// Scales about `point` (in the layer's coordinates).
gfx::Transform ScaleAbout(const gfx::Point& point, float scale) {
  gfx::Transform transform;
  transform.Translate(point.x(), point.y());
  transform.Scale(scale, scale);
  transform.Translate(-point.x(), -point.y());
  return transform;
}

class Backdrop : public views::View {
  METADATA_HEADER(Backdrop, views::View)

 public:
  explicit Backdrop(base::RepeatingClosure on_click)
      : on_click_(std::move(on_click)) {
    SetBackground(views::CreateSolidBackground(kBackdrop));
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    on_click_.Run();
    return true;
  }

 private:
  base::RepeatingClosure on_click_;
};

BEGIN_METADATA(Backdrop)
END_METADATA

}  // namespace

// The dimmed page, and the card with its buttons and the preview.
class GlanceView : public views::View {
  METADATA_HEADER(GlanceView, views::View)

 public:
  GlanceView(Glance* glance, content::WebContents* contents) {
    backdrop_ = AddChildView(std::make_unique<Backdrop>(
        base::BindRepeating(&Glance::Close, base::Unretained(glance))));

    auto card = std::make_unique<views::View>();
    card->SetBackground(
        views::CreateRoundedRectBackground(ui::kColorDialogBackground, kRadius));
    card->SetPaintToLayer();
    card->layer()->SetFillsBoundsOpaquely(false);
    auto* layout = card->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));

    auto header = std::make_unique<views::View>();
    header->SetLayoutManager(std::make_unique<views::BoxLayout>(
                                 views::BoxLayout::Orientation::kHorizontal,
                                 gfx::Insets::VH(4, 8), 4))
        ->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kEnd);
    auto as_tab = views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(&Glance::OpenAsTab, base::Unretained(glance)),
        kOpenInNewIcon, kIconSize);
    as_tab->SetTooltipText(u"Открыть как вкладку");
    header->AddChildView(std::move(as_tab));
    auto close = views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(&Glance::Close, base::Unretained(glance)),
        vector_icons::kCloseIcon, kIconSize);
    close->SetTooltipText(u"Закрыть (Esc)");
    header->AddChildView(std::move(close));
    card->AddChildView(std::move(header));

    auto web_view = std::make_unique<views::WebView>(contents->GetBrowserContext());
    web_view->SetWebContents(contents);
    web_view_ = card->AddChildView(std::move(web_view));
    layout->SetFlexForView(web_view_, 1);
    card_ = AddChildView(std::move(card));
  }

  GlanceView(const GlanceView&) = delete;
  GlanceView& operator=(const GlanceView&) = delete;
  ~GlanceView() override = default;

  // Grows the card from `screen_point`, Zen's openGlance.
  void AnimateIn(const gfx::Point& screen_point) {
    GetWidget()->LayoutRootViewIfNecessary();
    web_view_->holder()->SetNativeViewCornerRadii(
        gfx::RoundedCornersF(0, 0, kRadius, kRadius));
    gfx::Point origin = screen_point;
    views::View::ConvertPointFromScreen(card_, &origin);
    // The page appears when the card is in place.
    web_view_->SetVisible(false);
    card_->layer()->SetTransform(ScaleAbout(origin, 0.1f));
    card_->layer()->SetOpacity(0.f);
    backdrop_->layer()->SetOpacity(0.f);
    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&GlanceView::FinishIn,
                                weak_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(kOpenDuration)
        .SetTransform(card_, gfx::Transform(), gfx::Tween::FAST_OUT_SLOW_IN)
        .SetOpacity(card_, 1.f, gfx::Tween::LINEAR)
        .SetOpacity(backdrop_, 1.f, gfx::Tween::LINEAR);
    safety_.Start(FROM_HERE, kOpenDuration + kSafetyMargin,
                  base::BindOnce(&GlanceView::FinishIn,
                                 weak_factory_.GetWeakPtr()));
  }

  // Shrinks and fades the card, Zen's closeGlance, then runs `done`.
  void AnimateOut(base::OnceClosure done) {
    done_ = std::move(done);
    shown_ = true;
    safety_.Stop();
    web_view_->SetVisible(false);
    const gfx::Point centre = card_->GetLocalBounds().CenterPoint();
    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&GlanceView::FinishOut,
                                weak_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(kCloseDuration)
        .SetTransform(card_, ScaleAbout(centre, 0.92f), gfx::Tween::EASE_IN)
        .SetOpacity(card_, 0.f, gfx::Tween::LINEAR)
        .SetOpacity(backdrop_, 0.f, gfx::Tween::LINEAR);
    safety_.Start(FROM_HERE, kCloseDuration + kSafetyMargin,
                  base::BindOnce(&GlanceView::FinishOut,
                                 weak_factory_.GetWeakPtr()));
  }

  // Before the contents become a tab.
  void ReleaseContents() { web_view_->SetWebContents(nullptr); }

  // views::View:
  void Layout(PassKey) override {
    backdrop_->SetBoundsRect(GetLocalBounds());
    gfx::Rect card = GetLocalBounds();
    card.Inset(gfx::Insets::VH(card.height() * kCardInset,
                               card.width() * kCardInset));
    card_->SetBoundsRect(card);
  }

 private:
  void FinishIn() {
    if (shown_) {
      return;
    }
    shown_ = true;
    safety_.Stop();
    card_->layer()->GetAnimator()->StopAnimating();
    backdrop_->layer()->GetAnimator()->StopAnimating();
    card_->layer()->SetTransform(gfx::Transform());
    card_->layer()->SetOpacity(1.f);
    backdrop_->layer()->SetOpacity(1.f);
    web_view_->SetVisible(true);
    web_view_->RequestFocus();
  }

  void FinishOut() {
    safety_.Stop();
    if (done_) {
      std::move(done_).Run();
    }
  }

  raw_ptr<views::View> backdrop_ = nullptr;
  raw_ptr<views::View> card_ = nullptr;
  raw_ptr<views::WebView> web_view_ = nullptr;
  bool shown_ = false;
  base::OnceClosure done_;
  base::OneShotTimer safety_;
  base::WeakPtrFactory<GlanceView> weak_factory_{this};
};

BEGIN_METADATA(GlanceView)
END_METADATA

// static
void Glance::Open(BrowserWindowInterface* browser,
                  const GURL& url,
                  const gfx::Point& screen_point) {
  Glances().erase(browser);
  Glances()[browser] = base::WrapUnique(new Glance(browser, url, screen_point));
}

// static
Glance* Glance::Get(BrowserWindowInterface* browser) {
  auto it = Glances().find(browser);
  return it == Glances().end() ? nullptr : it->second.get();
}

Glance::Glance(BrowserWindowInterface* browser,
               const GURL& url,
               const gfx::Point& screen_point)
    : browser_(browser) {
  contents_ = content::WebContents::Create(
      content::WebContents::CreateParams(browser->GetProfile()));
  contents_->SetDelegate(this);
  content::NavigationController::LoadURLParams load(url);
  load.transition_type = ui::PAGE_TRANSITION_LINK;
  if (tabs::TabInterface* opener = browser->GetActiveTabInterface()) {
    load.referrer =
        content::Referrer(opener->GetContents()->GetLastCommittedURL(),
                          network::mojom::ReferrerPolicy::kDefault);
  }
  contents_->GetController().LoadURLWithParams(load);

  views::Widget* browser_widget = views::Widget::GetWidgetForNativeWindow(
      browser->GetWindow()->GetNativeWindow());
  delegate_ = std::make_unique<views::WidgetDelegate>();
  view_ = delegate_->SetContentsView(
      std::make_unique<GlanceView>(this, contents_.get()));
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = delegate_.get();
  params.parent = browser_widget->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.bounds = PageBounds();
  params.name = "CromarcGlance";
  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  widget_->Show();
  widget_->Activate();
  view_->AnimateIn(screen_point);

  browser_widget_observation_.Observe(browser_widget);
  browser->GetTabStripModel()->AddObserver(this);
  browser_did_close_ = browser->RegisterBrowserDidClose(
      base::BindRepeating([](BrowserWindowInterface* closed) {
        // Destroys this Glance.
        Glances().erase(closed);
      }));
}

Glance::~Glance() {
  browser_widget_observation_.Reset();
  view_ = nullptr;
  widget_.reset();
}

gfx::Rect Glance::PageBounds() const {
  tabs::TabInterface* tab = browser_->GetActiveTabInterface();
  return tab ? tab->GetContents()->GetContainerBounds()
             : browser_->GetWindow()->GetBounds();
}

void Glance::Close() {
  if (closing_) {
    return;
  }
  closing_ = true;
  view_->AnimateOut(
      base::BindOnce(&Glance::Destroy, weak_factory_.GetWeakPtr()));
}

void Glance::OpenAsTab() {
  if (closing_) {
    return;
  }
  closing_ = true;
  view_->ReleaseContents();
  contents_->SetDelegate(nullptr);
  TabStripModel* model = browser_->GetTabStripModel();
  model->InsertWebContentsAt(
      model->active_index() + 1, std::move(contents_),
      AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER);
  Destroy();
}

void Glance::Destroy() {
  widget_->Hide();
  // Not from inside the animation or the button that called this.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<Glance> glance) {
                       if (glance) {
                         Glances().erase(glance->browser_);
                       }
                     },
                     weak_factory_.GetWeakPtr()));
}

content::WebContents* Glance::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (params.disposition == WindowOpenDisposition::CURRENT_TAB) {
    source->GetController().LoadURLWithParams(
        content::NavigationController::LoadURLParams(params));
    return source;
  }
  // Links that ask for a new tab or window leave the preview.
  browser_->OpenGURL(params.url, params.disposition);
  return nullptr;
}

content::WebContents* Glance::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  return chrome::AddWebContents(browser_, source, std::move(new_contents),
                                target_url, disposition, window_features);
}

void Glance::CloseContents(content::WebContents* source) {
  Close();
}

content::KeyboardEventProcessingResult Glance::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown &&
      event.windows_key_code == ui::VKEY_ESCAPE) {
    Close();
    return content::KeyboardEventProcessingResult::HANDLED;
  }
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

void Glance::OnWidgetBoundsChanged(views::Widget* widget,
                                   const gfx::Rect& new_bounds) {
  widget_->SetBounds(PageBounds());
}

void Glance::OnTabStripModelChanged(TabStripModel* tab_strip_model,
                                    const TabStripModelChange& change,
                                    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    Close();
  }
}

}  // namespace cromarc
