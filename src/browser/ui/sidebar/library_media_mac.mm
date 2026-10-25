// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/ui/sidebar/library_media.h"

#import <AppKit/AppKit.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>

#include <optional>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace cromarc {

void GenerateThumbnail(
    const base::FilePath& path,
    int size,
    base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)> callback) {
  // QuickLook answers on a queue of its own; the PNG goes back to the UI
  // thread. A block copies what it holds: the callback rides in a refcount.
  using Reply = base::OnceCallback<void(scoped_refptr<base::RefCountedMemory>)>;
  auto reply = base::MakeRefCounted<base::RefCountedData<Reply>>(
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(callback)));
  NSURL* url = base::apple::FilePathToNSURL(path);
  QLThumbnailGenerationRequest* request = [[QLThumbnailGenerationRequest alloc]
        initWithFileAtURL:url
                     size:CGSizeMake(size, size)
                    scale:2.0
      representationTypes:QLThumbnailGenerationRequestRepresentationTypeAll];
  [QLThumbnailGenerator.sharedGenerator
      generateBestRepresentationForRequest:request
                         completionHandler:^(
                             QLThumbnailRepresentation* thumbnail,
                             NSError* error) {
                           scoped_refptr<base::RefCountedMemory> png;
                           if (thumbnail && thumbnail.CGImage) {
                             SkBitmap bitmap =
                                 skia::CGImageToSkBitmap(thumbnail.CGImage);
                             std::optional<std::vector<uint8_t>> bytes =
                                 gfx::PNGCodec::EncodeBGRASkBitmap(
                                     bitmap, /*discard_transparency=*/false);
                             if (bytes) {
                               png = base::MakeRefCounted<
                                   base::RefCountedBytes>(std::move(*bytes));
                             }
                           }
                           std::move(reply->data).Run(std::move(png));
                         }];
}

std::string AppNameToOpen(const base::FilePath& path) {
  NSURL* app = [NSWorkspace.sharedWorkspace
      URLForApplicationToOpenURL:base::apple::FilePathToNSURL(path)];
  if (!app) {
    return std::string();
  }
  return base::SysNSStringToUTF8(
      [NSFileManager.defaultManager displayNameAtPath:app.path]
          .stringByDeletingPathExtension);
}

}  // namespace cromarc
