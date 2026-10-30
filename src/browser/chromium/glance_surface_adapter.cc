// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/chromium/glance_surface_adapter.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"  // nogncheck: //chrome/browser/ui links //cromarc
#include "chrome/browser/ui/views/frame/contents_container_view.h"  // nogncheck: as above
#include "chrome/browser/ui/views/frame/multi_contents_view.h"  // nogncheck: as above
#include "components/input/native_web_keyboard_event.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "cromarc/browser/chromium/browser_frame_adapter.h"
#include "cromarc/browser/chromium/system_accent.h"
#include "cromarc/browser/ui/sidebar/cromarc_sidebar.h"
#include "cromarc/strings/grit/cromarc_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace cromarc {

namespace {

// Zen's zen.glance.animation-duration; closing is quicker.
constexpr base::TimeDelta kOpenDuration = base::Milliseconds(350);
constexpr base::TimeDelta kCloseDuration = base::Milliseconds(200);
constexpr int kFrameRate = 60;
// The card takes 85% of the page area.
constexpr float kCardInset = 0.075f;
// Zen's GLANCE_BACKGROUND_SCALE: the parent page behind, a little smaller.
constexpr float kParentScale = 0.97f;
constexpr float kCardRadius = 12.f;
constexpr SkAlpha kDim = 0x59;
// Zen's .zen-glance-sidebar-container: beside the card, 15px from its top.
constexpr int kButtonsGap = 12;
// Zen's column sits 15px down the card, with 12px of padding.
constexpr int kButtonsTop = 15 + 12;
// --size-item-large.
constexpr int kButtonSize = 34;
constexpr int kIconSize = 16;
// Zen's glance buttons on a light and on a dark toolbar, and hovered.
constexpr SkColor kButtonOnLight = SkColorSetRGB(24, 24, 24);
constexpr SkColor kButtonOnDark = SkColorSetRGB(231, 231, 231);
constexpr SkColor kButtonHoveredOnLight = SkColorSetRGB(41, 41, 41);
constexpr SkColor kButtonHoveredOnDark = SkColorSetRGB(204, 204, 204);
// How much of the primary colour the button's neutral takes.
constexpr float kButtonPrimaryMix = 0.04f;
// Zen's waitconfirmation on the close button: red, its label sliding out to
// at most 4rem, 8px from the icon, over 0.2 s.
constexpr SkColor kConfirmColor = SkColorSetRGB(220, 53, 69);
constexpr int kConfirmLabelMaxWidth = 64;
constexpr int kConfirmLabelGap = 8;
constexpr base::TimeDelta kConfirmSlide = base::Milliseconds(200);

BrowserView* ViewOf(BrowserWindowInterface* window) {
  return BrowserView::GetBrowserViewForBrowser(window);
}

gfx::Rect CardOf(const gfx::Rect& space) {
  gfx::Rect card = space;
  card.Inset(gfx::Insets::VH(space.height() * kCardInset,
                             space.width() * kCardInset));
  return card;
}

// Where the parent's page was, in `view`'s page area coordinates.
gfx::Rect ParentPageIn(BrowserView* browser_view) {
  gfx::Rect page = browser_view->multi_contents_view()->GetLocalBounds();
  page.Inset(PageInsets(browser_view));
  return page;
}

gfx::RectF ScaledAbout(const gfx::Rect& rect, float scale) {
  gfx::RectF scaled(rect);
  const gfx::PointF centre = scaled.CenterPoint();
  scaled.set_size(gfx::SizeF(rect.width() * scale, rect.height() * scale));
  scaled.set_origin(gfx::PointF(centre.x() - scaled.width() / 2,
                                centre.y() - scaled.height() / 2));
  return scaled;
}

SkPath Rounded(const gfx::RectF& rect, float radius) {
  return SkPath::RRect(SkRRect::MakeRectXY(gfx::RectFToSkRect(rect), radius,
                                           radius));
}

// The parent's page, scaled by `scale` about its centre, dimmed by `dim`.
void PaintParent(gfx::Canvas* canvas,
                 const gfx::ImageSkia* picture,
                 const gfx::Rect& page,
                 float scale,
                 SkAlpha dim) {
  const gfx::RectF rect = ScaledAbout(page, scale);
  gfx::ScopedCanvas scoped(canvas);
  canvas->ClipPath(Rounded(rect, kPageCornerRadius), /*do_anti_alias=*/true);
  if (picture && !picture->isNull()) {
    const gfx::Rect to = gfx::ToRoundedRect(rect);
    canvas->DrawImageInt(*picture, 0, 0, picture->width(), picture->height(),
                         to.x(), to.y(), to.width(), to.height(),
                         /*filter=*/true);
  }
  canvas->DrawColor(SkColorSetA(SK_ColorBLACK, dim));
}

// What the page area shows while a card grows or shrinks: the parent's page
// behind, dimming, and the card as a plain rounded sheet; the real page shows
// once it stands still.
class GlanceAnimationView : public views::View, public gfx::AnimationDelegate {
  METADATA_HEADER(GlanceAnimationView, views::View)

 public:
  GlanceAnimationView(const gfx::ImageSkia* picture,
                      gfx::Rect parent_page,
                      gfx::Rect card,
                      std::optional<gfx::Point> origin,
                      bool opening,
                      base::OnceClosure done)
      : picture_(picture ? *picture : gfx::ImageSkia()),
        parent_page_(parent_page),
        card_(card),
        origin_(origin),
        opening_(opening),
        done_(std::move(done)),
        animation_(opening ? kOpenDuration : kCloseDuration, kFrameRate, this) {
  }

  void Start() { animation_.Start(); }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    const double t = animation_.GetCurrentValue();
    if (opening_) {
      const double grow = gfx::Tween::CalculateValue(
          gfx::Tween::FAST_OUT_SLOW_IN, t);
      PaintParent(canvas, &picture_, parent_page_,
                  gfx::Tween::FloatValueBetween(grow, 1.f, kParentScale),
                  gfx::Tween::IntValueBetween(t, 0, kDim));
      const gfx::Rect from = origin_ ? gfx::Rect(*origin_, gfx::Size(1, 1))
                                     : gfx::Rect(card_.CenterPoint(),
                                                 gfx::Size(1, 1));
      PaintCard(canvas, gfx::Tween::RectValueBetween(grow, from, card_),
                gfx::Tween::IntValueBetween(t, 0, 255));
      return;
    }
    const double shrink =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, t);
    PaintParent(canvas, &picture_, parent_page_,
                gfx::Tween::FloatValueBetween(shrink, kParentScale, 1.f),
                gfx::Tween::IntValueBetween(t, kDim, 0));
    PaintCard(canvas,
              gfx::ToRoundedRect(ScaledAbout(
                  card_, gfx::Tween::FloatValueBetween(shrink, 1.f, 0.92f))),
              gfx::Tween::IntValueBetween(t, 255, 0));
  }

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override {
    SchedulePaint();
  }
  void AnimationEnded(const gfx::Animation* animation) override {
    if (done_) {
      std::move(done_).Run();
    }
  }

 private:
  void PaintCard(gfx::Canvas* canvas, const gfx::Rect& rect, int alpha) {
    const ui::ColorProvider* colors = GetColorProvider();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SkColorSetA(
        colors ? colors->GetColor(ui::kColorDialogBackground) : SK_ColorWHITE,
        alpha));
    canvas->DrawRoundRect(gfx::RectF(rect), kCardRadius, flags);
  }

  const gfx::ImageSkia picture_;
  const gfx::Rect parent_page_;
  const gfx::Rect card_;
  const std::optional<gfx::Point> origin_;
  const bool opening_;
  base::OnceClosure done_;
  gfx::LinearAnimation animation_;
};

BEGIN_METADATA(GlanceAnimationView)
END_METADATA

content::WebContents* ContentsOf(TabId tab) {
  tabs::TabInterface* interface = tabs::TabHandle(tab.value()).Get();
  return interface ? interface->GetContents() : nullptr;
}

// Esc in a glance's page, its top frame, as Zen's ZenGlanceChild hears it:
// before the page, which gets it too.
class EscapeWatcher : public content::WebContentsObserver {
 public:
  EscapeWatcher(content::WebContents* contents,
                base::RepeatingCallback<void(bool in_field)> on_escape)
      : content::WebContentsObserver(contents),
        on_escape_(std::move(on_escape)),
        callback_(base::BindRepeating(&EscapeWatcher::OnKey,
                                      base::Unretained(this))) {
    Attach(contents->GetPrimaryMainFrame());
  }
  ~EscapeWatcher() override { Attach(nullptr); }

  // content::WebContentsObserver:
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override {
    if (new_host && new_host->IsInPrimaryMainFrame()) {
      Attach(new_host);
    }
  }

 private:
  // By id: the old frame may be gone by the time it is let go.
  void Attach(content::RenderFrameHost* frame) {
    if (content::RenderFrameHost* old = content::RenderFrameHost::FromID(frame_)) {
      old->GetRenderWidgetHost()->RemoveKeyPressEventCallback(callback_);
    }
    frame_ = frame ? frame->GetGlobalId() : content::GlobalRenderFrameHostId();
    if (frame) {
      frame->GetRenderWidgetHost()->AddKeyPressEventCallback(callback_);
    }
  }

  bool OnKey(const input::NativeWebKeyboardEvent& event) {
    if (event.windows_key_code == ui::VKEY_ESCAPE &&
        !(event.GetModifiers() & blink::WebInputEvent::kKeyModifiers)) {
      // After the key, not inside its dispatch: closing the glance closes
      // this page.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(on_escape_,
                                    web_contents()->IsFocusedElementEditable()));
    }
    return false;
  }

  const base::RepeatingCallback<void(bool)> on_escape_;
  const content::RenderWidgetHost::KeyPressEventCallback callback_;
  content::GlobalRenderFrameHostId frame_;
};

// A glance button, as Zen's (.zen-glance-sidebar-container toolbarbutton):
// round, its icon in the primary colour (the system accent) on a neutral that
// contrasts with the toolbar. The close button can ask to confirm: red, with "Confirm" beside
// its icon (.zen-glance-sidebar-close[waitconfirmation]).
class GlanceButton : public views::Button {
  METADATA_HEADER(GlanceButton, views::Button)

 public:
  GlanceButton(PressedCallback callback,
               const gfx::VectorIcon& icon,
               int tooltip,
               base::RepeatingClosure resized)
      : views::Button(std::move(callback)),
        icon_(icon),
        resized_(std::move(resized)),
        label_(l10n_util::GetStringUTF16(IDS_CROMARC_GLANCE_CONFIRM)),
        label_width_(std::min(kConfirmLabelMaxWidth,
                              gfx::GetStringWidth(label_, font_))) {
    slide_.SetSlideDuration(kConfirmSlide);
    slide_.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    SetTooltipText(l10n_util::GetStringUTF16(tooltip));
  }

  // Asks for `duration`, then is a plain button again.
  void AskToConfirm(base::TimeDelta duration) {
    confirming_ = true;
    slide_.Show();
    SchedulePaint();
    stop_asking_.Start(
        FROM_HERE, duration,
        base::BindOnce(&GlanceButton::StopAsking, base::Unretained(this)));
  }

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const double t = slide_.GetCurrentValue();
    return gfx::Size(
        kButtonSize + base::ClampRound(t * (kConfirmLabelGap + label_width_)),
        kButtonSize);
  }
  void StateChanged(ButtonState old_state) override { SchedulePaint(); }
  void PaintButtonContents(gfx::Canvas* canvas) override {
    const ui::ColorProvider* colors = GetColorProvider();
    if (!colors) {
      return;
    }
    const SkColor primary = SystemAccentColor();
    const bool dark =
        color_utils::IsDark(colors->GetColor(ui::kColorSysSurface));
    SkColor background =
        color_utils::AlphaBlend(primary, dark ? kButtonOnDark : kButtonOnLight,
                                kButtonPrimaryMix);
    if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED) {
      background = dark ? kButtonHoveredOnDark : kButtonHoveredOnLight;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(confirming_ ? kConfirmColor : background);
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), kButtonSize / 2.f,
                          flags);
    const int inset = (kButtonSize - kIconSize) / 2;
    {
      gfx::ScopedCanvas scoped(canvas);
      canvas->Translate(gfx::Vector2d(inset, inset));
      gfx::PaintVectorIcon(canvas, *icon_, kIconSize,
                           confirming_ ? SK_ColorWHITE : primary);
    }
    const int shown = width() - kButtonSize - kConfirmLabelGap;
    if (shown > 0) {
      canvas->DrawStringRectWithFlags(
          label_, font_, SK_ColorWHITE,
          gfx::Rect(kButtonSize - inset + kConfirmLabelGap, 0, shown,
                    kButtonSize),
          gfx::Canvas::NO_ELLIPSIS);
    }
  }

  // gfx::AnimationDelegate, Button's too:
  void AnimationProgressed(const gfx::Animation* animation) override {
    if (animation != &slide_) {
      views::Button::AnimationProgressed(animation);
      return;
    }
    PreferredSizeChanged();
    resized_.Run();
  }

 private:
  void StopAsking() {
    confirming_ = false;
    slide_.Hide();
    SchedulePaint();
  }

  const raw_ref<const gfx::VectorIcon> icon_;
  const base::RepeatingClosure resized_;
  const gfx::FontList font_;
  const std::u16string label_;
  const int label_width_;
  bool confirming_ = false;
  gfx::SlideAnimation slide_{this};
  base::OneShotTimer stop_asking_;
};

BEGIN_METADATA(GlanceButton)
END_METADATA

std::unique_ptr<views::Widget> ChildWidget(views::Widget* browser_widget,
                                           const gfx::Rect& bounds,
                                           bool accept_events,
                                           views::WidgetDelegate* delegate,
                                           const char* name) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = delegate;
  params.parent = browser_widget->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = accept_events;
  params.bounds = bounds;
  params.name = name;
  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  return widget;
}

}  // namespace

// One window's glance while it shows.
class GlanceSurfaceAdapter::Shown : public ui::EventHandler,
                                    public views::ViewObserver {
 public:
  Shown(GlanceSurfaceAdapter& adapter,
        BrowserWindowInterface* window,
        TabId glance,
        TabId parent,
        Client* client)
      : adapter_(adapter),
        window_(window),
        browser_view_(ViewOf(window)),
        glance_(glance),
        parent_(parent),
        client_(client) {
    MultiContentsView* contents = browser_view_->multi_contents_view();
    contents->AddPreTargetHandler(this);
    contents_observation_.Observe(contents);
    if (views::View* container = contents->GetActiveContentsContainerView()) {
      card_observation_.Observe(container);
    }
    WatchEscape();
    Relayout();
  }

  ~Shown() override { Detach(); }

  TabId glance() const { return glance_; }
  TabId parent() const { return parent_; }

  void Update(TabId glance, TabId parent, Client* client) {
    glance_ = glance;
    parent_ = parent;
    client_ = client;
    WatchEscape();
    Relayout();
  }

  void AskToConfirmClose(base::TimeDelta duration) {
    if (close_button_) {
      close_button_->AskToConfirm(duration);
      return;
    }
    // Asked while the card grows: the button asks once it is there.
    confirm_until_ = base::TimeTicks::Now() + duration;
  }

  // Grows the card out of `origin`, then the buttons come.
  void GrowFrom(std::optional<gfx::Point> origin) {
    if (!origin || !browser_view_) {
      ShowButtons();
      return;
    }
    gfx::Point point = *origin;
    views::View::ConvertPointFromScreen(browser_view_->multi_contents_view(),
                                        &point);
    Animate(/*opening=*/true, point,
            base::BindOnce(&Shown::OnGrown, weak_factory_.GetWeakPtr()));
  }

  // Shrinks the card away, then `done`.
  void ShrinkAway(base::OnceClosure done) {
    close_button_ = nullptr;
    buttons_.reset();
    Animate(/*opening=*/false, std::nullopt, std::move(done));
  }

  // ui::EventHandler: a click beside the card closes the glance.
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() != ui::EventType::kMousePressed || !browser_view_) {
      return;
    }
    gfx::Point point = event->location();
    views::View::ConvertPointToTarget(
        static_cast<views::View*>(event->target()),
        browser_view_->multi_contents_view(), &point);
    if (!CardBounds().Contains(point)) {
      event->SetHandled();
      client_->CloseGlance(window_);
    }
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* view) override { PlaceButtons(); }
  void OnViewIsDeleting(views::View* view) override {
    Detach();
    // The window is going: this goes too, unless another showing took its
    // place meanwhile (compared, never followed).
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<GlanceSurfaceAdapter> adapter,
               BrowserWindowInterface* window, uintptr_t self) {
              if (!adapter) {
                return;
              }
              auto it = adapter->shown_.find(window);
              if (it != adapter->shown_.end() &&
                  reinterpret_cast<uintptr_t>(it->second.get()) == self) {
                adapter->shown_.erase(it);
              }
            },
            adapter_->weak_factory_.GetWeakPtr(), window_.get(),
            reinterpret_cast<uintptr_t>(this)));
  }

 private:
  void WatchEscape() {
    content::WebContents* contents = ContentsOf(glance_);
    escape_ = contents ? std::make_unique<EscapeWatcher>(
                             contents, base::BindRepeating(
                                           &Shown::OnEscape,
                                           weak_factory_.GetWeakPtr()))
                       : nullptr;
  }

  void OnEscape(bool in_field) { client_->EscapeGlance(window_, in_field); }

  gfx::Rect CardBounds() const {
    views::View* container =
        browser_view_->multi_contents_view()->GetActiveContentsContainerView();
    return container ? container->bounds() : gfx::Rect();
  }

  void Relayout() {
    if (!browser_view_) {
      return;
    }
    browser_view_->multi_contents_view()->InvalidateLayout();
    if (views::Widget* widget = browser_view_->GetWidget()) {
      widget->non_client_view()->frame_view()->SchedulePaint();
    }
  }

  void Animate(bool opening,
               std::optional<gfx::Point> origin,
               base::OnceClosure done) {
    if (!browser_view_) {
      std::move(done).Run();
      return;
    }
    MultiContentsView* contents = browser_view_->multi_contents_view();
    // The layout the card ends in, before the first frame.
    contents->GetWidget()->LayoutRootViewIfNecessary();
    animation_delegate_ = std::make_unique<views::WidgetDelegate>();
    GlanceAnimationView* view = animation_delegate_->SetContentsView(
        std::make_unique<GlanceAnimationView>(
            adapter_->PictureOf(parent_), ParentPageIn(browser_view_),
            CardBounds(), origin, opening, std::move(done)));
    animation_ = ChildWidget(contents->GetWidget(),
                             contents->GetBoundsInScreen(),
                             /*accept_events=*/false, animation_delegate_.get(),
                             "CromarcGlanceAnimation");
    animation_->ShowInactive();
    view->Start();
  }

  void OnGrown() {
    animation_.reset();
    animation_delegate_.reset();
    ShowButtons();
  }

  void ShowButtons() {
    if (!browser_view_ || buttons_) {
      return;
    }
    buttons_delegate_ = std::make_unique<views::WidgetDelegate>();
    auto column = std::make_unique<views::View>();
    column->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical, gfx::Insets(),
        kButtonsGap));
    // Zen's column is left-aligned: the close button grows to the right.
    static_cast<views::BoxLayout*>(column->GetLayoutManager())
        ->set_cross_axis_alignment(
            views::BoxLayout::CrossAxisAlignment::kStart);
    const base::RepeatingClosure resized =
        base::BindRepeating(&Shown::PlaceButtons, base::Unretained(this));
    close_button_ = column->AddChildView(std::make_unique<GlanceButton>(
        base::BindRepeating(&Shown::Close, base::Unretained(this)),
        vector_icons::kCloseIcon, IDS_CROMARC_GLANCE_CLOSE, resized));
    column->AddChildView(std::make_unique<GlanceButton>(
        base::BindRepeating(&Shown::Expand, base::Unretained(this)),
        kOpenInNewIcon, IDS_CROMARC_GLANCE_OPEN_AS_TAB, resized));
    if (const base::TimeDelta left = confirm_until_ - base::TimeTicks::Now();
        left.is_positive()) {
      close_button_->AskToConfirm(left);
    }
    buttons_delegate_->SetContentsView(std::move(column));
    buttons_ = ChildWidget(browser_view_->GetWidget(), gfx::Rect(),
                           /*accept_events=*/true, buttons_delegate_.get(),
                           "CromarcGlanceButtons");
    PlaceButtons();
  }

  void PlaceButtons() {
    if (!buttons_ || !browser_view_) {
      return;
    }
    const gfx::Size size =
        buttons_->GetContentsView()->GetPreferredSize();
    gfx::Rect card = CardBounds();
    views::View::ConvertRectToScreen(browser_view_->multi_contents_view(),
                                     &card);
    gfx::Rect bounds(
        gfx::Point(card.right() + kButtonsGap, card.y() + kButtonsTop), size);
    // In the window, as Zen's: a label sliding out is cut at its edge, and
    // buttons beside a card at the window's edge do not show.
    bounds.Intersect(browser_view_->GetWidget()->GetWindowBoundsInScreen());
    if (bounds.IsEmpty()) {
      buttons_->Hide();
      return;
    }
    buttons_->SetBounds(bounds);
    if (!buttons_->IsVisible()) {
      buttons_->ShowInactive();
    }
  }

  // After the click: closing or expanding takes the buttons away, and they
  // must not go inside their own click.
  void Close() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<Shown> shown) {
                         if (shown) {
                           shown->client_->CloseGlance(shown->window_);
                         }
                       },
                       weak_factory_.GetWeakPtr()));
  }
  void Expand() {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](base::WeakPtr<Shown> shown) {
                         if (shown) {
                           shown->client_->ExpandGlance(shown->window_);
                         }
                       },
                       weak_factory_.GetWeakPtr()));
  }

  void Detach() {
    escape_.reset();
    close_button_ = nullptr;
    buttons_.reset();
    animation_.reset();
    card_observation_.Reset();
    if (!browser_view_) {
      return;
    }
    browser_view_->multi_contents_view()->RemovePreTargetHandler(this);
    contents_observation_.Reset();
    Relayout();
    browser_view_ = nullptr;
  }

  const raw_ref<GlanceSurfaceAdapter> adapter_;
  const raw_ptr<BrowserWindowInterface> window_;
  raw_ptr<BrowserView> browser_view_;
  TabId glance_;
  TabId parent_;
  raw_ptr<Client> client_;
  std::unique_ptr<views::WidgetDelegate> animation_delegate_;
  std::unique_ptr<views::Widget> animation_;
  std::unique_ptr<views::WidgetDelegate> buttons_delegate_;
  std::unique_ptr<views::Widget> buttons_;
  raw_ptr<GlanceButton> close_button_ = nullptr;
  // Until when the close button is to ask, when asked before it was there.
  base::TimeTicks confirm_until_;
  std::unique_ptr<EscapeWatcher> escape_;
  base::ScopedObservation<views::View, views::ViewObserver>
      contents_observation_{this};
  base::ScopedObservation<views::View, views::ViewObserver> card_observation_{
      this};
  base::WeakPtrFactory<Shown> weak_factory_{this};
};

GlanceSurfaceAdapter::GlanceSurfaceAdapter() = default;
GlanceSurfaceAdapter::~GlanceSurfaceAdapter() = default;

std::optional<gfx::Rect> GlanceSurfaceAdapter::CardIn(BrowserView* browser_view,
                                                      const gfx::Rect& space) {
  if (!shown_.contains(browser_view->browser())) {
    return std::nullopt;
  }
  return CardOf(space);
}

void GlanceSurfaceAdapter::PaintBackdrop(gfx::Canvas* canvas,
                                         BrowserView* browser_view) {
  auto it = shown_.find(browser_view->browser());
  if (it == shown_.end() || !browser_view->multi_contents_view()) {
    return;
  }
  views::View* frame = browser_view->GetWidget()->non_client_view()->frame_view();
  const gfx::Rect page = views::View::ConvertRectToTarget(
      browser_view->multi_contents_view(), frame, ParentPageIn(browser_view));
  PaintParent(canvas, PictureOf(it->second->parent()), page, kParentScale,
              kDim);
}

void GlanceSurfaceAdapter::Snapshot(TabId tab) {
  content::WebContents* contents = ContentsOf(tab);
  content::RenderWidgetHostView* view =
      contents ? contents->GetPrimaryMainFrame()->GetView() : nullptr;
  if (!view) {
    return;
  }
  // The copy may answer on another sequence; the pictures live on this one.
  view->CopyFromSurface(
      gfx::Rect(), gfx::Size(), base::Seconds(1),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          [](base::WeakPtr<GlanceSurfaceAdapter> adapter, TabId tab,
             const content::CopyFromSurfaceResult& result) {
            if (!adapter || !result.has_value() || result->bitmap.drawsNothing()) {
              return;
            }
            adapter->pictures_[tab] =
                gfx::ImageSkia::CreateFrom1xBitmap(result->bitmap);
            // A glance may already show over it.
            for (const auto& [window, shown] : adapter->shown_) {
              if (shown->parent() == tab) {
                if (BrowserView* browser_view = ViewOf(window)) {
                  browser_view->GetWidget()
                      ->non_client_view()
                      ->frame_view()
                      ->SchedulePaint();
                }
              }
            }
          },
          weak_factory_.GetWeakPtr(), tab)));
}

void GlanceSurfaceAdapter::Forget(TabId tab) {
  pictures_.erase(tab);
}

void GlanceSurfaceAdapter::Show(BrowserWindowInterface* window,
                                TabId glance,
                                TabId parent,
                                std::optional<gfx::Point> origin,
                                Client* client) {
  if (!ViewOf(window) || !ViewOf(window)->multi_contents_view()) {
    return;
  }
  std::unique_ptr<Shown>& shown = shown_[window];
  if (shown) {
    shown->Update(glance, parent, client);
  } else {
    shown = std::make_unique<Shown>(*this, window, glance, parent, client);
  }
  shown->GrowFrom(origin);
}

void GlanceSurfaceAdapter::AskToConfirmClose(BrowserWindowInterface* window,
                                             base::TimeDelta duration) {
  if (auto it = shown_.find(window); it != shown_.end()) {
    it->second->AskToConfirmClose(duration);
  }
}

void GlanceSurfaceAdapter::Hide(BrowserWindowInterface* window,
                                bool animate,
                                base::OnceClosure done) {
  auto it = shown_.find(window);
  if (it == shown_.end()) {
    std::move(done).Run();
    return;
  }
  if (!animate) {
    shown_.erase(it);
    std::move(done).Run();
    return;
  }
  it->second->ShrinkAway(base::BindOnce(
      [](base::WeakPtr<GlanceSurfaceAdapter> adapter,
         BrowserWindowInterface* window, base::OnceClosure done) {
        if (adapter) {
          adapter->shown_.erase(window);
        }
        std::move(done).Run();
      },
      weak_factory_.GetWeakPtr(), window, std::move(done)));
}

const gfx::ImageSkia* GlanceSurfaceAdapter::PictureOf(TabId tab) const {
  auto it = pictures_.find(tab);
  return it == pictures_.end() ? nullptr : &it->second;
}

GlanceSurfaceAdapter& ChromiumGlanceSurface() {
  static base::NoDestructor<GlanceSurfaceAdapter> surface;
  return *surface;
}

}  // namespace cromarc
