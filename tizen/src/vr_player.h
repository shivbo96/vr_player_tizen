#ifndef FLUTTER_PLUGIN_VR_PLAYER_H_
#define FLUTTER_PLUGIN_VR_PLAYER_H_

#include <Ecore.h>
#include <Eina.h>
#include <flutter/encodable_value.h>
#include <flutter/event_channel.h>
#include <flutter/plugin_registrar.h>
#include <flutter/texture_registrar.h>
#include <player.h>

#define VR_PLAYER_HAS_360 1

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

namespace vr_player_tizen {

class VrPlayer {
public:
  explicit VrPlayer(flutter::PluginRegistrar *plugin_registrar,
                    flutter::TextureRegistrar *texture_registrar);
  ~VrPlayer();

  void LoadVideo(const std::string &uri);
  void Play();
  void Pause();
  void SetVolume(double volume);
  void SeekTo(int32_t position);
  bool IsPlaying();
  int32_t GetPosition();
  void Dispose();

  void SetVRMode(bool enabled);
  void ToggleVRMode();
  void StartContinuousDrag(double dx, double dy);
  void StopContinuousDrag();

  int64_t GetTextureId() { return texture_id_; }

private:
  void InitializePlayer();
  void SetUpEventChannel(flutter::BinaryMessenger *messenger);
  void SendPendingEvents();
  void PushEvent(const std::pair<std::string, flutter::EncodableValue> &event);

  FlutterDesktopGpuSurfaceDescriptor *ObtainGpuSurface(size_t width,
                                                       size_t height);
  static void OnPrepared(void *data);
  static void OnCompleted(void *data);
  static void OnError(int error_code, void *data);
  static Eina_Bool OnPositionTimer(void *data);
  static void OnVideoFrameDecoded(media_packet_h packet, void *data);
  static void ReleaseMediaPacket(void *packet);
  static Eina_Bool OnDragTimer(void *data);

  void RequestRendering();
  void OnRenderingCompleted();

  media_packet_h current_media_packet_ = nullptr;
  media_packet_h previous_media_packet_ = nullptr;

  bool is_rendering_ = false;
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
  int64_t texture_id_ = -1;
  std::string uri_;

  flutter::TextureRegistrar *texture_registrar_;
  std::unique_ptr<flutter::TextureVariant> texture_variant_;
  std::unique_ptr<FlutterDesktopGpuSurfaceDescriptor> gpu_surface_;
  std::mutex mutex_;

  Ecore_Pipe *sink_event_pipe_ = nullptr;
  std::mutex queue_mutex_;
  std::queue<std::pair<std::string, flutter::EncodableValue>>
      encodable_event_queue_;
  Ecore_Timer *position_timer_ = nullptr;

  bool is_360_enabled_ = false;
  float yaw_ = 0.0f;
  float pitch_ = 0.0f;
  float drag_dx_ = 0.0f;
  float drag_dy_ = 0.0f;
  Ecore_Timer *drag_timer_ = nullptr;
};

} // namespace vr_player_tizen

#endif // FLUTTER_PLUGIN_VR_PLAYER_H_
