#include "vr_player_plugin.h"

#include <dlog.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#if __has_include(<flutter_tizen.h>)
#include <flutter_tizen.h>
#endif

#include <map>
#include <memory>
#include <string>

#include "vr_player.h"
#include "vr_player_view.h"

#define LOG_ERROR(fmt, ...)                                                    \
  dlog_print(DLOG_ERROR, "VRPlayerPlugin", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                     \
  dlog_print(DLOG_INFO, "VRPlayerPlugin", fmt, ##__VA_ARGS__)

namespace {

class VrPlayerPlugin : public flutter::Plugin {
public:
  static void
  RegisterWithRegistrar(flutter::PluginRegistrar *registrar,
                        FlutterDesktopPluginRegistrarRef core_registrar) {
    auto plugin = std::make_unique<VrPlayerPlugin>(registrar);

#if VR_PLAYER_HAS_PLATFORM_VIEW
    auto factory = std::make_unique<vr_player_tizen::VrPlayerViewFactory>(
        registrar, [plugin_ptr = plugin.get()](int64_t view_id) {
          return plugin_ptr->GetPlayer(view_id);
        });
    FlutterDesktopRegisterViewFactory(
        core_registrar, "plugins.vr_player/player_view", std::move(factory));
#else
    // Log a warning if PlatformView support is missing at compile-time.
    // This happens if flutter/platform_view.h is not found.
    LOG_ERROR(
        "VR Player: PlatformView support is missing in this build profile.");
#endif

    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "vr_player",
            &flutter::StandardMethodCodec::GetInstance());

    channel->SetMethodCallHandler(
        [plugin_ptr = plugin.get()](const auto &call, auto result) {
          plugin_ptr->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  VrPlayerPlugin(flutter::PluginRegistrar *registrar) : registrar_(registrar) {}
  virtual ~VrPlayerPlugin() {}

private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const std::string &method = method_call.method_name();

    if (method == "init") {
      // PlatformViews handle their own ID generation.
      // We return 0 as a placeholder, but the real initialization
      // happens when the TizenView is created.
      result->Success(flutter::EncodableValue(0));
      return;
    }

    if (method == "getPlatformVersion") {
      result->Success(flutter::EncodableValue("Tizen"));
    } else {
      result->NotImplemented();
    }
  }

  void HandlePlayerMethodCall(
      int64_t texture_id,
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    auto it = players_.find(texture_id);
    if (it == players_.end()) {
      result->Error("Not_Found", "Player not found");
      return;
    }

    vr_player_tizen::VrPlayer *player = it->second.get();
    const std::string &method = method_call.method_name();

    if (method == "loadVideo") {
      const auto *args =
          std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (args) {
        auto url_it = args->find(flutter::EncodableValue("videoUrl"));
        if (url_it != args->end() &&
            std::holds_alternative<std::string>(url_it->second)) {
          player->LoadVideo(std::get<std::string>(url_it->second));
        } else {
          auto path_it = args->find(flutter::EncodableValue("videoPath"));
          if (path_it != args->end() &&
              std::holds_alternative<std::string>(path_it->second)) {
            player->LoadVideo(std::get<std::string>(path_it->second));
          }
        }
      }
      result->Success();
    } else if (method == "play") {
      player->Play();
      result->Success();
    } else if (method == "pause") {
      player->Pause();
      result->Success();
    } else if (method == "setVolume") {
      const auto *args =
          std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (args) {
        auto vol_it = args->find(flutter::EncodableValue("volume"));
        if (vol_it != args->end()) {
          double volume = 0.0;
          if (std::holds_alternative<double>(vol_it->second)) {
            volume = std::get<double>(vol_it->second);
          } else if (std::holds_alternative<int32_t>(vol_it->second)) {
            volume = static_cast<double>(std::get<int32_t>(vol_it->second));
          }
          player->SetVolume(volume);
        }
      }
      result->Success();
    } else if (method == "seekTo") {
      const auto *args =
          std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (args) {
        auto pos_it = args->find(flutter::EncodableValue("position"));
        if (pos_it != args->end()) {
          int32_t position = 0;
          if (std::holds_alternative<int32_t>(pos_it->second)) {
            position = std::get<int32_t>(pos_it->second);
          } else if (std::holds_alternative<int64_t>(pos_it->second)) {
            position = static_cast<int32_t>(std::get<int64_t>(pos_it->second));
          } else if (std::holds_alternative<double>(pos_it->second)) {
            position = static_cast<int32_t>(std::get<double>(pos_it->second));
          }
          player->SeekTo(position);
        }
      }
      result->Success();
    } else if (method == "dispose") {
      player_channels_.erase(texture_id);
      players_.erase(texture_id);
      result->Success();
    } else if (method == "isPlaying") {
      result->Success(flutter::EncodableValue(player->IsPlaying()));
    } else if (method == "toggleVRMode") {
      player->SetVRMode(!player->Is360Enabled());
      result->Success();
    } else if (method == "setVRMode") {
      const auto *args =
          std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (args) {
        auto enabled_it = args->find(flutter::EncodableValue("enabled"));
        if (enabled_it != args->end() &&
            std::holds_alternative<bool>(enabled_it->second)) {
          player->SetVRMode(std::get<bool>(enabled_it->second));
        }
      }
      result->Success();
    } else if (method == "startContinuousDrag") {
      const auto *args =
          std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (args) {
        double dx = 0, dy = 0;
        auto dx_it = args->find(flutter::EncodableValue("dx"));
        auto dy_it = args->find(flutter::EncodableValue("dy"));
        if (dx_it != args->end() &&
            std::holds_alternative<double>(dx_it->second)) {
          dx = std::get<double>(dx_it->second);
        }
        if (dy_it != args->end() &&
            std::holds_alternative<double>(dy_it->second)) {
          dy = std::get<double>(dy_it->second);
        }
        player->StartContinuousDrag(dx, dy);
      }
      result->Success();
    } else if (method == "stopContinuousDrag") {
      player->StopContinuousDrag();
      result->Success();
    } else {
      result->Success(); // stub out unsupported methods
    }
  }

  void EnsurePlayer(int64_t id) {
    if (players_.find(id) == players_.end()) {
      auto player = std::make_unique<vr_player_tizen::VrPlayer>(registrar_);
      players_[id] = std::move(player);

      std::string id_str = std::to_string(id);
      auto channel =
          std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
              registrar_->messenger(), "vr_player_" + id_str,
              &flutter::StandardMethodCodec::GetInstance());

      channel->SetMethodCallHandler([this, id](const auto &call, auto result) {
        HandlePlayerMethodCall(id, call, std::move(result));
      });

      player_channels_[id] = std::move(channel);
    }
  }

  vr_player_tizen::VrPlayer *GetPlayer(int64_t id) {
    EnsurePlayer(id);
    return players_[id].get();
  }

  flutter::PluginRegistrar *registrar_;
  std::map<int64_t, std::unique_ptr<vr_player_tizen::VrPlayer>> players_;
  std::map<int64_t,
           std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>>>
      player_channels_;
};

} // namespace

void VrPlayerPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  VrPlayerPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrar>(registrar),
      registrar);
}
