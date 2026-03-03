#include "vr_player.h"

#include <dlog.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/standard_method_codec.h>

#define LOG_ERROR(fmt, ...)                                                    \
  dlog_print(DLOG_ERROR, "VRPlayerPlugin", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                     \
  dlog_print(DLOG_INFO, "VRPlayerPlugin", fmt, ##__VA_ARGS__)

namespace vr_player_tizen {

VrPlayer::VrPlayer(flutter::PluginRegistrar *plugin_registrar,
                   flutter::TextureRegistrar *texture_registrar)
    : texture_registrar_(texture_registrar) {
  gpu_surface_ = std::make_unique<FlutterDesktopGpuSurfaceDescriptor>();

  texture_variant_ =
      std::make_unique<flutter::TextureVariant>(flutter::GpuSurfaceTexture(
          kFlutterDesktopGpuSurfaceTypeNone,
          [this](size_t width,
                 size_t height) -> const FlutterDesktopGpuSurfaceDescriptor * {
            return this->ObtainGpuSurface(width, height);
          }));
  texture_id_ = texture_registrar_->RegisterTexture(texture_variant_.get());

  sink_event_pipe_ = ecore_pipe_add(
      [](void *data, void *buffer, unsigned int nbyte) {
        auto *player = static_cast<VrPlayer *>(data);
        player->SendPendingEvents();
      },
      this);

  SetUpEventChannel(plugin_registrar->messenger());
  InitializePlayer();
}

VrPlayer::~VrPlayer() {
  Dispose();
  if (sink_event_pipe_) {
    ecore_pipe_del(sink_event_pipe_);
    sink_event_pipe_ = nullptr;
  }
}

void VrPlayer::InitializePlayer() {
  int ret = player_create(&player_);
  if (ret != PLAYER_ERROR_NONE) {
    LOG_ERROR("player_create failed: %d", ret);
    return;
  }

  player_set_prepared_cb(player_, OnPrepared, this);
  player_set_completed_cb(player_, OnCompleted, this);
  player_set_error_cb(player_, OnError, this);

  player_set_display_mode(player_, PLAYER_DISPLAY_MODE_DST_ROI);
}

void VrPlayer::LoadVideo(const std::string &uri) {
  uri_ = uri;
  int ret = player_set_uri(player_, uri_.c_str());
  if (ret != PLAYER_ERROR_NONE) {
    LOG_ERROR("player_set_uri failed: %d", ret);
    return;
  }

  ret = player_set_video_stream_changed_cb(player_, OnVideoFrameDecoded, this);
  if (ret != PLAYER_ERROR_NONE) {
    LOG_ERROR("player_set_video_stream_changed_cb failed: %d", ret);
  }

  ret = player_prepare_async(player_);
  if (ret != PLAYER_ERROR_NONE) {
    LOG_ERROR("player_prepare_async failed: %d", ret);
  }
}

void VrPlayer::Play() {
  if (player_) {
    player_start(player_);
    PushEvent({"state",
               flutter::EncodableValue(1)}); // VrState.ready or playing, based
                                             // on your lib logic. 3=idle, etc.
  }
}

void VrPlayer::Pause() {
  if (player_) {
    player_pause(player_);
    PushEvent({"state", flutter::EncodableValue(3)}); // idle
  }
}

void VrPlayer::SetVolume(double volume) {
  if (player_) {
    player_set_volume(player_, volume, volume);
  }
}

void VrPlayer::SeekTo(int32_t position) {
  if (player_) {
    player_set_play_position(player_, position, true, nullptr, nullptr);
  }
}

int32_t VrPlayer::GetPosition() {
  if (!player_)
    return 0;
  int position = 0;
  player_get_play_position(player_, &position);
  return position;
}

void VrPlayer::Dispose() {
  if (player_) {
    player_stop(player_);
    player_unprepare(player_);
    player_destroy(player_);
    player_ = nullptr;
  }
  if (texture_id_ != -1) {
    texture_registrar_->UnregisterTexture(texture_id_);
    texture_id_ = -1;
  }
}

void VrPlayer::SetUpEventChannel(flutter::BinaryMessenger *messenger) {
  std::string state_channel_name =
      "vr_player_events_" + std::to_string(texture_id_) + "_state";
  state_channel_ =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          messenger, state_channel_name,
          &flutter::StandardMethodCodec::GetInstance());
  auto state_handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [this](
          const flutter::EncodableValue *arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        state_sink_ = std::move(events);
        return nullptr;
      },
      [this](const flutter::EncodableValue *arguments)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        state_sink_ = nullptr;
        return nullptr;
      });
  state_channel_->SetStreamHandler(std::move(state_handler));

  std::string position_channel_name =
      "vr_player_events_" + std::to_string(texture_id_) + "_position";
  position_channel_ =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          messenger, position_channel_name,
          &flutter::StandardMethodCodec::GetInstance());
  auto position_handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [this](
          const flutter::EncodableValue *arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        position_sink_ = std::move(events);
        return nullptr;
      },
      [this](const flutter::EncodableValue *arguments)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        position_sink_ = nullptr;
        return nullptr;
      });
  position_channel_->SetStreamHandler(std::move(position_handler));

  std::string duration_channel_name =
      "vr_player_events_" + std::to_string(texture_id_) + "_duration";
  duration_channel_ =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          messenger, duration_channel_name,
          &flutter::StandardMethodCodec::GetInstance());
  auto duration_handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [this](
          const flutter::EncodableValue *arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        duration_sink_ = std::move(events);
        return nullptr;
      },
      [this](const flutter::EncodableValue *arguments)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        duration_sink_ = nullptr;
        return nullptr;
      });
  duration_channel_->SetStreamHandler(std::move(duration_handler));

  std::string ended_channel_name =
      "vr_player_events_" + std::to_string(texture_id_) + "_ended";
  ended_channel_ =
      std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
          messenger, ended_channel_name,
          &flutter::StandardMethodCodec::GetInstance());
  auto ended_handler = std::make_unique<
      flutter::StreamHandlerFunctions<flutter::EncodableValue>>(
      [this](
          const flutter::EncodableValue *arguments,
          std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        ended_sink_ = std::move(events);
        return nullptr;
      },
      [this](const flutter::EncodableValue *arguments)
          -> std::unique_ptr<
              flutter::StreamHandlerError<flutter::EncodableValue>> {
        ended_sink_ = nullptr;
        return nullptr;
      });
  ended_channel_->SetStreamHandler(std::move(ended_handler));
}

void VrPlayer::SendPendingEvents() {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  while (!encodable_event_queue_.empty()) {
    auto event = encodable_event_queue_.front();
    if (event.first == "state" && state_sink_) {
      flutter::EncodableMap map;
      map[flutter::EncodableValue("state")] = event.second;
      state_sink_->Success(flutter::EncodableValue(map));
    } else if (event.first == "position" && position_sink_) {
      flutter::EncodableMap map;
      map[flutter::EncodableValue("currentPosition")] = event.second;
      position_sink_->Success(flutter::EncodableValue(map));
    } else if (event.first == "duration" && duration_sink_) {
      flutter::EncodableMap map;
      map[flutter::EncodableValue("duration")] = event.second;
      duration_sink_->Success(flutter::EncodableValue(map));
    } else if (event.first == "ended" && ended_sink_) {
      flutter::EncodableMap map;
      map[flutter::EncodableValue("ended")] = event.second;
      ended_sink_->Success(flutter::EncodableValue(map));
    }
    encodable_event_queue_.pop();
  }
}

void VrPlayer::PushEvent(
    const std::pair<std::string, flutter::EncodableValue> &event) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  encodable_event_queue_.push(event);
  if (sink_event_pipe_) {
    ecore_pipe_write(sink_event_pipe_, nullptr, 0);
  }
}

FlutterDesktopGpuSurfaceDescriptor *VrPlayer::ObtainGpuSurface(size_t width,
                                                               size_t height) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!current_media_packet_) {
    is_rendering_ = false;
    OnRenderingCompleted();
    return nullptr;
  }

  tbm_surface_h surface;
  int ret = media_packet_get_tbm_surface(current_media_packet_, &surface);
  if (ret != MEDIA_PACKET_ERROR_NONE || !surface) {
    LOG_ERROR("Failed to get a tbm surface, error: %d", ret);
    is_rendering_ = false;
    media_packet_destroy(current_media_packet_);
    current_media_packet_ = nullptr;
    OnRenderingCompleted();
    return nullptr;
  }
  gpu_surface_->handle = surface;
  gpu_surface_->width = width;
  gpu_surface_->height = height;
  gpu_surface_->release_context = this;
  gpu_surface_->release_callback = ReleaseMediaPacket;
  return gpu_surface_.get();
}

void VrPlayer::OnPrepared(void *data) {
  auto *player = static_cast<VrPlayer *>(data);
  player->PushEvent({"state", flutter::EncodableValue(1)}); // VrState.ready

  int duration = 0;
  player_get_duration(player->player_, &duration);
  player->PushEvent({"duration", flutter::EncodableValue(duration)});
}

void VrPlayer::OnCompleted(void *data) {
  auto *player = static_cast<VrPlayer *>(data);
  player->PushEvent({"ended", flutter::EncodableValue(true)});
}

void VrPlayer::OnError(int error_code, void *data) {
  LOG_ERROR("Player error: %d", error_code);
}

void VrPlayer::OnVideoFrameDecoded(media_packet_h packet, void *data) {
  auto *player = static_cast<VrPlayer *>(data);

  std::lock_guard<std::mutex> lock(player->mutex_);
  if (player->previous_media_packet_) {
    media_packet_destroy(player->previous_media_packet_);
  }
  player->previous_media_packet_ = player->current_media_packet_;
  player->current_media_packet_ = packet;
  player->RequestRendering();
}

void VrPlayer::ReleaseMediaPacket(void *data) {
  auto *player = static_cast<VrPlayer *>(data);

  std::lock_guard<std::mutex> lock(player->mutex_);
  player->is_rendering_ = false;
  player->OnRenderingCompleted();
}

void VrPlayer::RequestRendering() {
  if (!is_rendering_) {
    is_rendering_ = true;
    texture_registrar_->MarkTextureFrameAvailable(texture_id_);
  }
}

void VrPlayer::OnRenderingCompleted() {
  if (current_media_packet_ && !is_rendering_) {
    is_rendering_ = true;
    texture_registrar_->MarkTextureFrameAvailable(texture_id_);
  }
}

} // namespace vr_player_tizen
