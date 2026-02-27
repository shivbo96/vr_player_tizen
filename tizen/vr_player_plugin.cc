#include "vr_player_plugin.h"

#include <flutter/event_channel.h>
#include <flutter/method_channel.h>
#include <flutter/platform_view_factory.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <sstream>
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

class VrPlayerView : public flutter::PlatformView {
public:
  VrPlayerView(flutter::BinaryMessenger *messenger, int64_t view_id,
               const flutter::EncodableMap &args)
      : view_id_(view_id) {
    std::string channel_name = "vr_player_" + std::to_string(view_id);
    method_channel_ =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            messenger, channel_name,
            &flutter::StandardMethodCodec::GetInstance());

    method_channel_->SetMethodCallHandler(
        [this](const auto &call, auto result) {
          HandleMethodCall(call, std::move(result));
        });

    // Setup event channels
    setupEventChannel(messenger, "state");
    setupEventChannel(messenger, "duration");
    setupEventChannel(messenger, "position");
    setupEventChannel(messenger, "ended");
  }

  virtual ~VrPlayerView() {}

  virtual void *GetWindow() override { return nullptr; }

private:
  void setupEventChannel(flutter::BinaryMessenger *messenger,
                         const std::string &type) {
    std::string channel_name =
        "vr_player_events_" + std::to_string(view_id_) + "_" + type;
    auto event_channel =
        std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
            messenger, channel_name,
            &flutter::StandardMethodCodec::GetInstance());
    auto handler = std::make_unique<VrPlayerStreamHandler>();
    event_channel->SetStreamHandler(std::move(handler));
    event_channels_.push_back(std::move(event_channel));
  }

  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const std::string &method_name = method_call.method_name();

    // These methods should match those in VrPlayerController.dart
    if (method_name == "loadVideo") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "play") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "pause") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "isPlaying") {
      result->Success(flutter::EncodableValue(false));
    } else if (method_name == "setVolume") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "seekTo") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "dispose") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "toggleVRMode") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "onSizeChanged") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "onPause") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "onResume") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "onOrientationChanged") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "simulateTouch") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "simulateDrag") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "startContinuousDrag") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "stopContinuousDrag") {
      result->Success(flutter::EncodableValue(true));
    } else if (method_name == "fullScreen") {
      result->Success(flutter::EncodableValue(true));
    } else {
      result->NotImplemented();
    }
  }

  int64_t view_id_;
  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>>
      method_channel_;
  std::vector<std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>>
      event_channels_;
};

class VrPlayerViewFactory : public flutter::PlatformViewFactory {
public:
  VrPlayerViewFactory(flutter::BinaryMessenger *messenger)
      : messenger_(messenger) {}

  virtual flutter::PlatformView *
  Create(int64_t view_id, double width, double height,
         const flutter::EncodableMap &args) override {
    return new VrPlayerView(messenger_, view_id, args);
  }

private:
  flutter::BinaryMessenger *messenger_;
};

class VrPlayerPluginImpl : public VrPlayerPlugin {
public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar) {
    auto channel =
        std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
            registrar->messenger(), "vr_player",
            &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<VrPlayerPluginImpl>();

    channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto &call, auto result) {
          plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->RegisterViewFactory(
        "plugins.vr_player/player_view",
        std::make_unique<VrPlayerViewFactory>(registrar->messenger()));

    registrar->AddPlugin(std::move(plugin));
  }

  VrPlayerPluginImpl() {}

  virtual ~VrPlayerPluginImpl() {}

private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const std::string &method_name = method_call.method_name();

    if (method_name == "getPlatformVersion") {
      result->Success(flutter::EncodableValue("Tizen"));
    } else {
      result->NotImplemented();
    }
  }
};

} // namespace

void VrPlayerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrar *registrar) {
  VrPlayerPluginImpl::RegisterWithRegistrar(registrar);
}

VrPlayerPlugin::VrPlayerPlugin() {}

VrPlayerPlugin::~VrPlayerPlugin() {}
