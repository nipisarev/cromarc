// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// The strings native code shows, read through l10n_util from the paks the
// build makes, in English and in Russian.

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "cromarc/browser/arc_import/arc_import.h"
#include "cromarc/strings/grit/cromarc_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace cromarc {
namespace {

class CromarcStringsTest : public testing::TestWithParam<const char*> {
 protected:
  void SetUp() override {
    base::FilePath gen;
    ASSERT_TRUE(base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &gen));
    const std::string locale = GetParam();
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    bundle.OverrideLocalePakForTest(gen.AppendASCII(
        "cromarc/strings/cromarc_strings_" + locale + ".pak"));
    bundle.ReloadLocaleResources(locale);
  }

  // Back to the strings the test suite started with (ui_test.pak).
  void TearDown() override {
    base::FilePath ui_test_pak;
    ASSERT_TRUE(base::PathService::Get(ui::UI_TEST_PAK, &ui_test_pak));
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    bundle.OverrideLocalePakForTest(ui_test_pak);
    bundle.ReloadLocaleResources(std::string());
    bundle.OverrideLocalePakForTest(base::FilePath());
  }

  bool russian() const { return std::string(GetParam()) == "ru"; }

  std::string Text(int message) {
    return base::UTF16ToUTF8(l10n_util::GetStringUTF16(message));
  }
};

TEST_P(CromarcStringsTest, NativeViewsReadInTheLocale) {
  EXPECT_EQ(Text(IDS_CROMARC_LITTLE_OPEN_IN_ANOTHER),
            russian() ? "Открыть в другом space" : "Open in another space");
  EXPECT_EQ(Text(IDS_CROMARC_LITTLE_CLOSE), russian()
                                                ? "Закрыть (уйдёт в архив)"
                                                : "Close (goes to the archive)");
  EXPECT_EQ(Text(IDS_CROMARC_GLANCE_OPEN_AS_TAB),
            russian() ? "Открыть как вкладку" : "Open as Tab");
  EXPECT_EQ(Text(IDS_CROMARC_GLANCE_CLOSE),
            russian() ? "Закрыть (Esc)" : "Close (Esc)");
  EXPECT_EQ(Text(IDS_CROMARC_ARC_IMPORT_EXPORT_FILE),
            russian() ? "Файл экспорта Arc" : "Arc Export File");
}

TEST_P(CromarcStringsTest, ArgumentsGoInAsTheyAre) {
  // A space's name is the user's: a "$1", quotes or an emoji in it stay.
  EXPECT_EQ(base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
                IDS_CROMARC_LITTLE_ALWAYS_OPEN, u"example.com",
                u"$1 «Работа» 🙂")),
            russian() ? "Всегда открывать example.com в «$1 «Работа» 🙂»"
                      : "Always open example.com in “$1 «Работа» 🙂”");
  EXPECT_EQ(base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
                IDS_CROMARC_LITTLE_OPEN_IN, u"$2 Work")),
            russian() ? "Открыть в $2 Work" : "Open in $2 Work");
}

TEST_P(CromarcStringsTest, ImportSummaryReadsLabelThenNumber) {
  ImportCounts counts;
  counts.pinned = 1;
  counts.essentials = 1234;
  EXPECT_EQ(base::UTF16ToUTF8(SummaryLine(u"Work", counts)),
            russian() ? "Work: Папки 0, Закреплённые 1, Важные 1,234, Today 0"
                      : "Work: Folders 0, Pinned 1, Essentials 1,234, Today 0");
  counts.skipped = 3;
  EXPECT_EQ(base::UTF16ToUTF8(SummaryLine(u"Work", counts)),
            russian() ? "Work: Папки 0, Закреплённые 1, Важные 1,234, Today 0, "
                        "Уже были 3"
                      : "Work: Folders 0, Pinned 1, Essentials 1,234, Today 0, "
                        "Already there 3");
}

INSTANTIATE_TEST_SUITE_P(, CromarcStringsTest, testing::Values("en-US", "ru"));

}  // namespace
}  // namespace cromarc
