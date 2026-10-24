// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "cromarc/browser/routing/space_routing.h"

#include <algorithm>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/token.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace cromarc {

namespace {

std::optional<Routes> Read(const base::FilePath& path) {
  std::string json;
  if (!base::ReadFileToString(path, &json)) {
    return Routes();
  }
  std::optional<base::Value> value =
      base::JSONReader::Read(json, base::JSON_PARSE_RFC);
  return value ? RoutesFromValue(*value) : std::nullopt;
}

void Write(const base::FilePath& path, std::string json) {
  base::ImportantFileWriter::WriteFileAtomically(path, json);
}

}  // namespace

// static
SpaceRouting* SpaceRouting::Get() {
  static base::NoDestructor<SpaceRouting> instance;
  return instance.get();
}

SpaceRouting::SpaceRouting()
    : file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      path_(g_browser_process->profile_manager()->user_data_dir().AppendASCII(
          "Cromarc Routes.json")) {
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&Read, path_),
      base::BindOnce(&SpaceRouting::OnRead, weak_factory_.GetWeakPtr()));
}

SpaceRouting::~SpaceRouting() = default;

void SpaceRouting::OnRead(std::optional<Routes> routes) {
  // An unreadable file is kept as it is; the rules start empty until saved.
  routes_ = routes.value_or(Routes());
  loaded_ = true;
  for (base::OnceClosure& callback : std::exchange(pending_, {})) {
    std::move(callback).Run();
  }
  on_changed_.Notify();
}

void SpaceRouting::WhenLoaded(base::OnceClosure callback) {
  if (loaded_) {
    std::move(callback).Run();
  } else {
    pending_.push_back(std::move(callback));
  }
}

void SpaceRouting::AddRoute(const std::string& reference,
                            Route::Match match,
                            const std::string& space) {
  routes_.routes.push_back(
      {base::Token::CreateRandom().ToString(), reference, match, space});
  Save();
}

void SpaceRouting::RemoveRoute(const std::string& id) {
  std::erase_if(routes_.routes,
                [&id](const Route& route) { return route.id == id; });
  Save();
}

void SpaceRouting::SetDefaultExternal(const std::string& space) {
  routes_.default_external = space;
  Save();
}

base::CallbackListSubscription SpaceRouting::RegisterOnChanged(
    base::RepeatingClosure callback) {
  return on_changed_.Add(std::move(callback));
}

void SpaceRouting::Save() {
  std::string json;
  base::JSONWriter::WriteWithOptions(RoutesToValue(routes_),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
  file_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&Write, path_, std::move(json)));
  on_changed_.Notify();
}

}  // namespace cromarc
