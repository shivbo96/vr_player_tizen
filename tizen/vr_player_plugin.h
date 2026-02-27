#ifndef FLUTTER_PLUGIN_VR_PLAYER_PLUGIN_H_
#define FLUTTER_PLUGIN_VR_PLAYER_PLUGIN_H_

#include <flutter/event_channel.h>
#include <flutter/method_channel.h>
#include <flutter/platform_view_factory.h>
#include <flutter/plugin_registrar.h>
#include <flutter/standard_method_codec.h>

#include <map>
#include <memory>
#include <string>

class VrPlayerPlugin : public flutter::Plugin {
public:
  static void RegisterWithRegistrar(flutter::PluginRegistrar *registrar);

  VrPlayerPlugin();

  virtual ~VrPlayerPlugin();

private:
  void HandleMethodCall(
      const flutter::MethodCall<flutter::EncodableValue> &method_call,
      std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result);
};

#endif // FLUTTER_PLUGIN_VR_PLAYER_PLUGIN_H_
