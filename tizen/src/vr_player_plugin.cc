#include "vr_player_plugin.h"

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <string>
#include <vector>

namespace {

class VrPlayerStreamHandler
    : public flutter::StreamHandler<flutter::EncodableValue> {
public:
  VrPlayerStreamHandler() {}
  virtual ~VrPlayerStreamHandler() {}

protected:
  std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
  OnListenInternal(const flutter::EncodableValue *arguments,
                   std::unique_ptr<flutter::EventSink<flutter::EncodableValue>>
                       events) override {
    events_ = std::move(events);
    return nullptr;
  }

  std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
  OnCancelInternal(const flutter::EncodableValue *arguments) override {
    events_.reset();
    return nullptr;
  }

private:
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> events_;
};

class VrPlayerPlugin : public flutter::Plugin {
public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar) {
    // Main plugin method channel
    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "vr_player",
            &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<VrPlayerPlugin>(registrar->messenger());

    channel->SetMethodCallHandler(
        [plugin_ptr = plugin.get()](const auto &call, auto result) {
          plugin_ptr->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  explicit VrPlayerPlugin(flutter::BinaryMessenger *messenger)
      : messenger_(messenger) {}
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

  flutter::BinaryMessenger *messenger_;
};

} // namespace

void VrPlayerPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  VrPlayerPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrar>(registrar));
}
