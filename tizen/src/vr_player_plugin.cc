#include "vr_player_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <string>

namespace {

class VrPlayerPlugin : public flutter::Plugin {
public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar) {
    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "vr_player",
            &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<VrPlayerPlugin>();

    channel->SetMethodCallHandler(
        [plugin_ptr = plugin.get()](const auto &call, auto result) {
          plugin_ptr->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  VrPlayerPlugin() {}
  virtual ~VrPlayerPlugin() {}

private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const std::string &method = method_call.method_name();

    if (method == "getPlatformVersion") {
      result->Success(flutter::EncodableValue("Tizen"));
    } else if (method == "loadVideo" || method == "play" || method == "pause" ||
               method == "setVolume" || method == "seekTo" ||
               method == "dispose" || method == "toggleVRMode" ||
               method == "onSizeChanged" || method == "onPause" ||
               method == "onResume" || method == "onOrientationChanged" ||
               method == "simulateTouch" || method == "simulateDrag" ||
               method == "startContinuousDrag" ||
               method == "stopContinuousDrag" || method == "fullScreen") {
      result->Success(flutter::EncodableValue(true));
    } else if (method == "isPlaying") {
      result->Success(flutter::EncodableValue(false));
    } else {
      result->NotImplemented();
    }
  }
};

} // namespace

void VrPlayerPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  VrPlayerPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrar>(registrar));
}
