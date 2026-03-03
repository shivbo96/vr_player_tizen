#include "vr_player_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <string>

#include "vr_player.h"

namespace {

class VrPlayerPlugin : public flutter::Plugin {
public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar) {
    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "vr_player",
            &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<VrPlayerPlugin>(registrar);

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
      auto player = std::make_unique<vr_player_tizen::VrPlayer>(
          registrar_, registrar_->texture_registrar());
      int64_t texture_id = player->GetTextureId();
      players_[texture_id] = std::move(player);

      // We also need to set up a specific channel for this player
      std::string id_str = std::to_string(texture_id);
      auto channel =
          std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
              registrar_->messenger(), "vr_player_" + id_str,
              &flutter::StandardMethodCodec::GetInstance());

      channel->SetMethodCallHandler(
          [this, texture_id](const auto &call, auto result) {
            HandlePlayerMethodCall(texture_id, call, std::move(result));
          });

      player_channels_[texture_id] = std::move(channel);

      result->Success(flutter::EncodableValue(texture_id));
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
        if (vol_it != args->end() &&
            std::holds_alternative<double>(vol_it->second)) {
          player->SetVolume(std::get<double>(vol_it->second));
        }
      }
      result->Success();
    } else if (method == "seekTo") {
      const auto *args =
          std::get_if<flutter::EncodableMap>(method_call.arguments());
      if (args) {
        auto pos_it = args->find(flutter::EncodableValue("position"));
        if (pos_it != args->end() &&
            std::holds_alternative<int32_t>(pos_it->second)) {
          player->SeekTo(std::get<int32_t>(pos_it->second));
        }
      }
      result->Success();
    } else if (method == "dispose") {
      player_channels_.erase(texture_id);
      players_.erase(texture_id);
      result->Success();
    } else if (method == "isPlaying") {
      result->Success(flutter::EncodableValue(true)); // stubbed
    } else {
      result->Success(); // stub out unsupported methods
    }
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
          ->GetRegistrar<flutter::PluginRegistrar>(registrar));
}
