#ifndef FLUTTER_PLUGIN_VR_PLAYER_VIEW_H_
#define FLUTTER_PLUGIN_VR_PLAYER_VIEW_H_

#include <flutter/platform_view.h>
#include <flutter/plugin_registrar.h>
#include <memory>
#include <string>

#include "vr_player.h"

namespace vr_player_tizen {

class VrPlayerView : public flutter::PlatformView {
public:
  VrPlayerView(flutter::PluginRegistrar *registrar, int64_t view_id,
               VrPlayer *player)
      : registrar_(registrar), view_id_(view_id), player_(player) {}

  virtual ~VrPlayerView() {}

  virtual void *GetNativeView() override {
    // Return an Evas_Object or similar native view handle.
    // For Tizen PlatformViews, we often use an image object or a sub-window.
    // However, the Player API can take a window handle directly if we use
    // OVERLAY.
    return registrar_->GetView();
  }

  void SetPlayerDisplay() {
    if (player_) {
      player_->SetDisplay(GetNativeView());
    }
  }

private:
  flutter::PluginRegistrar *registrar_;
  int64_t view_id_;
  VrPlayer *player_;
};

class VrPlayerViewFactory : public flutter::PlatformViewFactory {
public:
  VrPlayerViewFactory(flutter::PluginRegistrar *registrar,
                      std::function<VrPlayer *(int64_t)> player_provider)
      : flutter::PlatformViewFactory(registrar->messenger()),
        registrar_(registrar), player_provider_(player_provider) {}

  virtual std::unique_ptr<flutter::PlatformView>
  Create(int64_t view_id, double width, double height,
         const flutter::EncodableValue *creation_params) override {
    VrPlayer *player = player_provider_(view_id);
    auto view = std::make_unique<VrPlayerView>(registrar_, view_id, player);
    view->SetPlayerDisplay();
    return view;
  }

private:
  flutter::PluginRegistrar *registrar_;
  std::function<VrPlayer *(int64_t)> player_provider_;
};

} // namespace vr_player_tizen

#endif // FLUTTER_PLUGIN_VR_PLAYER_VIEW_H_
