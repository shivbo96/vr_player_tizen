#ifndef FLUTTER_PLUGIN_VR_PLAYER_H_
#define FLUTTER_PLUGIN_VR_PLAYER_H_

#include <Ecore.h>
#include <Eina.h>
#include <flutter/encodable_value.h>
#include <flutter/event_channel.h>
#include <flutter/plugin_registrar.h>
#include <player.h>

#if __has_include(<player_360.h>)
#include <player_360.h>
#define VR_PLAYER_HAS_360 1
#else
#define VR_PLAYER_HAS_360 0
#endif

#if __has_include(<player_display.h>)
#include <player_display.h>
#endif

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace vr_player_tizen {

class VrPlayer {
public:
  explicit VrPlayer(flutter::PluginRegistrar *plugin_registrar);
  ~VrPlayer();

  void LoadVideo(const std::string &uri);
  void Play();
  void Pause();
  void SetVolume(double volume);
  void SeekTo(int32_t position);
  int32_t GetPosition();
  bool IsPlaying();
  void SetVRMode(bool enabled);
  bool Is360Enabled() { return is_360_enabled_; }
  void StartContinuousDrag(double dx, double dy);
  void StopContinuousDrag();
  void Dispose();

  void SetDisplay(void *window_handle);

private:
  void InitializePlayer();
  void SetUpEventChannel(flutter::BinaryMessenger *messenger);
  void SendPendingEvents();
  void PushEvent(const std::pair<std::string, flutter::EncodableValue> &event);

  static void OnPrepared(void *data);
  static void OnCompleted(void *data);
  static void OnError(int error_code, void *data);
  static Eina_Bool OnPositionTimer(void *data);
  static Eina_Bool OnDragTimer(void *data);

  bool is_prepared_ = false;
  bool play_on_prepared_ = false;

  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      state_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> state_sink_;

  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      position_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> position_sink_;

  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      duration_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> duration_sink_;

  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>>
      ended_channel_;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> ended_sink_;

  player_h player_ = nullptr;
  std::string uri_;

  std::mutex mutex_;

  Ecore_Pipe *sink_event_pipe_ = nullptr;
  std::mutex queue_mutex_;
  std::queue<std::pair<std::string, flutter::EncodableValue>>
      encodable_event_queue_;
  Ecore_Timer *position_timer_ = nullptr;

  bool is_360_enabled_ = false;
  float yaw_ = 0.0f;
  float pitch_ = 0.0f;
  float zoom_ = 1.0f;
  float drag_dx_ = 0.0f;
  float drag_dy_ = 0.0f;
  Ecore_Timer *drag_timer_ = nullptr;
};

} // namespace vr_player_tizen

#endif // FLUTTER_PLUGIN_VR_PLAYER_H_
