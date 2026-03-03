#include "vr_player.h"

#include <dlog.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/standard_method_codec.h>
#include <memory>
#include <string>
#include <vector>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define LOG_ERROR(fmt, ...)                                                    \
  dlog_print(DLOG_ERROR, "VRPlayerPlugin", fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                     \
  dlog_print(DLOG_INFO, "VRPlayerPlugin", fmt, ##__VA_ARGS__)

namespace vr_player_tizen {

VrPlayer::VrPlayer(flutter::PluginRegistrar *plugin_registrar) {
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

  player_set_completed_cb(player_, OnCompleted, this);
  player_set_error_cb(player_, OnError, this);

  player_set_display_mode(player_, PLAYER_DISPLAY_MODE_DST_ROI);
  player_set_display_visible(player_, true);
}

void VrPlayer::LoadVideo(const std::string &uri) {
  uri_ = uri;
  int ret = player_set_uri(player_, uri_.c_str());
  if (ret != PLAYER_ERROR_NONE) {
    LOG_ERROR("player_set_uri failed: %d", ret);
    return;
  }

  ret = player_prepare_async(
      player_,
      [](void *user_data) {
        auto *player = static_cast<VrPlayer *>(user_data);
        player->OnPrepared(player);
      },
      this);

  if (ret != PLAYER_ERROR_NONE) {
    LOG_ERROR("player_prepare_async failed: %d", ret);
  }
}

void VrPlayer::Play() {
  if (player_) {
    if (!is_prepared_) {
      play_on_prepared_ = true;
      return;
    }
    player_start(player_);

    if (position_timer_) {
      ecore_timer_del(position_timer_);
    }
    position_timer_ = ecore_timer_add(0.5, OnPositionTimer, this);

    // Re-send duration when playback starts to ensure UI is in sync.
    int duration = 0;
    if (player_get_duration(player_, &duration) == PLAYER_ERROR_NONE) {
      PushEvent(std::make_pair("duration", flutter::EncodableValue(duration)));
    }

    PushEvent(std::make_pair(
        "state", flutter::EncodableValue(1))); // VrState.ready/playing
  }
}

void VrPlayer::Pause() {
  if (player_) {
    player_pause(player_);
    if (position_timer_) {
      ecore_timer_del(position_timer_);
      position_timer_ = nullptr;
    }
    PushEvent(
        std::make_pair("state", flutter::EncodableValue(3))); // idle/paused
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

bool VrPlayer::IsPlaying() {
  if (!player_)
    return false;
  player_state_e state;
  if (player_get_state(player_, &state) == PLAYER_ERROR_NONE) {
    return state == PLAYER_STATE_PLAYING;
  }
  return false;
}

int32_t VrPlayer::GetPosition() {
  if (!player_)
    return 0;
  int position = 0;
  player_get_play_position(player_, &position);
  return position;
}

void VrPlayer::Dispose() {
  if (position_timer_) {
    ecore_timer_del(position_timer_);
    position_timer_ = nullptr;
  }
  if (player_) {
    player_stop(player_);
    player_unprepare(player_);
    player_destroy(player_);
    player_ = nullptr;
  }
  if (drag_timer_) {
    ecore_timer_del(drag_timer_);
    drag_timer_ = nullptr;
  }
}

void VrPlayer::SetUpEventChannel(flutter::BinaryMessenger *messenger) {
  std::string state_channel_name = "vr_player_events_state";
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

  std::string position_channel_name = "vr_player_events_position";
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

  std::string duration_channel_name = "vr_player_events_duration";
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

  std::string ended_channel_name = "vr_player_events_ended";
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

void VrPlayer::SetDisplay(void *window_handle) {
  if (!player_)
    return;
  int ret =
      player_set_display(player_, PLAYER_DISPLAY_TYPE_OVERLAY, window_handle);
  if (ret != PLAYER_ERROR_NONE) {
    LOG_ERROR("player_set_display failed: %d", ret);
  }
}

void VrPlayer::OnPrepared(void *data) {
  auto *player = static_cast<VrPlayer *>(data);
  player->is_prepared_ = true;

  player->PushEvent(
      std::make_pair("state", flutter::EncodableValue(1))); // VrState.ready

  int duration = 0;
  player_get_duration(player->player_, &duration);
  player->PushEvent(
      std::make_pair("duration", flutter::EncodableValue(duration)));

  if (player->play_on_prepared_) {
    player->play_on_prepared_ = false;
    player->Play();
  }

  bool is_spherical = false;
  if (player_360_is_content_spherical(player->player_, &is_spherical) ==
          PLAYER_ERROR_NONE &&
      is_spherical) {
    LOG_INFO("Content is spherical, enabling 360 mode.");
    player->SetVRMode(true);
  }
}

void VrPlayer::OnCompleted(void *data) {
  auto *player = static_cast<VrPlayer *>(data);
  player->PushEvent(std::make_pair("ended", flutter::EncodableValue(true)));
}

void VrPlayer::OnError(int error_code, void *data) {
  LOG_ERROR("Player error: %d", error_code);
}

Eina_Bool VrPlayer::OnPositionTimer(void *data) {
  auto *player = static_cast<VrPlayer *>(data);
  int position = 0;
  player_get_play_position(player->player_, &position);
  player->PushEvent(
      std::make_pair("position", flutter::EncodableValue(position)));
  return ECORE_CALLBACK_RENEW;
}

void VrPlayer::OnRenderingCompleted() {}

void VrPlayer::SetVRMode(bool enabled) {
  if (!player_)
    return;
  int ret = player_360_set_enabled(player_, enabled);
  if (ret != PLAYER_ERROR_NONE) {
    LOG_ERROR("player_360_set_enabled failed: %d", ret);
    return;
  }
  is_360_enabled_ = enabled;
  LOG_INFO("360 mode %s", enabled ? "enabled" : "disabled");
}

void VrPlayer::StartContinuousDrag(double dx, double dy) {
  drag_dx_ = static_cast<float>(dx);
  drag_dy_ = static_cast<float>(dy);

  if (!drag_timer_) {
    drag_timer_ = ecore_timer_add(0.016, OnDragTimer, this); // ~60fps
  }
}

void VrPlayer::StopContinuousDrag() {
  if (drag_timer_) {
    ecore_timer_del(drag_timer_);
    drag_timer_ = nullptr;
  }
}

Eina_Bool VrPlayer::OnDragTimer(void *data) {
  auto *player = static_cast<VrPlayer *>(data);
  if (!player->player_ || !player->is_360_enabled_) {
    return ECORE_CALLBACK_RENEW;
  }

  // Adjust sensitivity: these are pixel deltas from Flutter, map to degrees.
  // Assuming a drag of 'width' pixels should be ~180 degrees.
  float sensitivity = 0.05f;
  player->yaw_ += player->drag_dx_ * sensitivity;
  player->pitch_ += player->drag_dy_ * sensitivity;

  // Clamp pitch to avoid flipping (-90 to 90)
  if (player->pitch_ > 90.0f)
    player->pitch_ = 90.0f;
  if (player->pitch_ < -90.0f)
    player->pitch_ = -90.0f;

  // Wrap yaw (0 to 360)
  while (player->yaw_ > 360.0f)
    player->yaw_ -= 360.0f;
  while (player->yaw_ < 0.0f)
    player->yaw_ += 360.0f;

  player_360_set_direction_of_view(player->player_, player->yaw_,
                                   player->pitch_);
  return ECORE_CALLBACK_RENEW;
}

} // namespace vr_player_tizen
