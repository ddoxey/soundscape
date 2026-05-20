#include "openal_audio_output.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace {

std::string AlErrorName(ALenum error) {
  switch (error) {
    case AL_NO_ERROR:
      return "AL_NO_ERROR";
    case AL_INVALID_NAME:
      return "AL_INVALID_NAME";
    case AL_INVALID_ENUM:
      return "AL_INVALID_ENUM";
    case AL_INVALID_VALUE:
      return "AL_INVALID_VALUE";
    case AL_INVALID_OPERATION:
      return "AL_INVALID_OPERATION";
    case AL_OUT_OF_MEMORY:
      return "AL_OUT_OF_MEMORY";
    default:
      return "Unknown OpenAL error " + std::to_string(error);
  }
}

std::string AlcErrorName(ALCenum error) {
  switch (error) {
    case ALC_NO_ERROR:
      return "ALC_NO_ERROR";
    case ALC_INVALID_DEVICE:
      return "ALC_INVALID_DEVICE";
    case ALC_INVALID_CONTEXT:
      return "ALC_INVALID_CONTEXT";
    case ALC_INVALID_ENUM:
      return "ALC_INVALID_ENUM";
    case ALC_INVALID_VALUE:
      return "ALC_INVALID_VALUE";
    case ALC_OUT_OF_MEMORY:
      return "ALC_OUT_OF_MEMORY";
    default:
      return "Unknown OpenAL context error " + std::to_string(error);
  }
}

}  // namespace

OpenAlAudioOutput::~OpenAlAudioOutput() { Shutdown(); }

bool OpenAlAudioOutput::Initialize(AudioRenderTarget& render_target,
                                   std::string& error_message) {
  render_target_ = &render_target;
  scratch_buffer_.resize(kBufferFramesPerChunk * kOutputChannels);
  pcm_buffer_.resize(kBufferFramesPerChunk * kOutputChannels);

  device_ = alcOpenDevice(nullptr);
  if (device_ == nullptr) {
    error_message = "Failed to open the default OpenAL device";
    render_target_ = nullptr;
    return false;
  }

  context_ = alcCreateContext(device_, nullptr);
  if (context_ == nullptr) {
    error_message = BuildAlcError("create OpenAL context");
    Shutdown();
    return false;
  }

  if (alcMakeContextCurrent(context_) == ALC_FALSE) {
    error_message = BuildAlcError("activate OpenAL context");
    Shutdown();
    return false;
  }

  alGenSources(1, &source_);
  ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    error_message = BuildAlError("create OpenAL source", error);
    Shutdown();
    return false;
  }

  alGenBuffers(static_cast<ALsizei>(buffers_.size()), buffers_.data());
  error = alGetError();
  if (error != AL_NO_ERROR) {
    error_message = BuildAlError("create OpenAL buffers", error);
    Shutdown();
    return false;
  }

  // Prime the OpenAL queue before starting playback so the worker thread has
  // time to refill processed buffers without an immediate underrun.
  for (ALuint buffer_id : buffers_) {
    if (!FillBuffer(buffer_id, &error_message)) {
      Shutdown();
      return false;
    }
  }

  alSourceQueueBuffers(source_, static_cast<ALsizei>(buffers_.size()),
                       buffers_.data());
  error = alGetError();
  if (error != AL_NO_ERROR) {
    error_message = BuildAlError("queue initial OpenAL buffers", error);
    Shutdown();
    return false;
  }

  alSourcePlay(source_);
  error = alGetError();
  if (error != AL_NO_ERROR) {
    error_message = BuildAlError("start OpenAL source", error);
    Shutdown();
    return false;
  }

  alcMakeContextCurrent(nullptr);
  running_ = true;
  worker_thread_ = std::thread(&OpenAlAudioOutput::WorkerLoop, this);
  return true;
}

void OpenAlAudioOutput::Shutdown() {
  running_ = false;
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }

  if (context_ != nullptr) {
    // All OpenAL object deletion happens with this backend's context current on
    // the calling thread.
    alcMakeContextCurrent(context_);
  }

  if (source_ != 0) {
    alSourceStop(source_);

    ALint queued_count = 0;
    alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued_count);
    while (queued_count > 0) {
      ALuint buffer_id = 0;
      alSourceUnqueueBuffers(source_, 1, &buffer_id);
      --queued_count;
    }

    alDeleteSources(1, &source_);
    source_ = 0;
  }

  const bool have_buffers =
      std::any_of(buffers_.begin(), buffers_.end(),
                  [](ALuint buffer_id) { return buffer_id != 0; });
  if (have_buffers) {
    alDeleteBuffers(static_cast<ALsizei>(buffers_.size()), buffers_.data());
    buffers_.fill(0);
  }

  if (context_ != nullptr) {
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context_);
    context_ = nullptr;
  }

  if (device_ != nullptr) {
    alcCloseDevice(device_);
    device_ = nullptr;
  }

  render_target_ = nullptr;
  scratch_buffer_.clear();
  pcm_buffer_.clear();
}

void OpenAlAudioOutput::WorkerLoop() {
  alcMakeContextCurrent(context_);

  while (running_) {
    ALint processed_count = 0;
    alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed_count);

    // OpenAL marks queued buffers as processed after playback consumes them.
    // Refill and requeue each processed buffer to produce a continuous stream.
    while (processed_count > 0) {
      ALuint buffer_id = 0;
      alSourceUnqueueBuffers(source_, 1, &buffer_id);

      if (FillBuffer(buffer_id, nullptr)) {
        alSourceQueueBuffers(source_, 1, &buffer_id);
      }

      --processed_count;
    }

    ALint source_state = AL_STOPPED;
    alGetSourcei(source_, AL_SOURCE_STATE, &source_state);
    if (source_state != AL_PLAYING) {
      // If the source underruns, restart it after queueing whatever buffers are
      // available.
      alSourcePlay(source_);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  alcMakeContextCurrent(nullptr);
}

bool OpenAlAudioOutput::FillBuffer(ALuint buffer_id,
                                   std::string* error_message) {
  {
    const std::lock_guard<std::mutex> lock(render_mutex_);
    if (render_target_ == nullptr) {
      std::fill(scratch_buffer_.begin(), scratch_buffer_.end(), 0.0f);
    } else {
      render_target_->Render(scratch_buffer_.data(), kBufferFramesPerChunk);
    }
  }

  // OpenAL Soft can support float buffers via extensions, but 16-bit stereo PCM
  // is the portable baseline and is sufficient for the POC output path.
  for (std::size_t i = 0; i < scratch_buffer_.size(); ++i) {
    const float sample = std::clamp(scratch_buffer_[i], -1.0f, 1.0f);
    pcm_buffer_[i] = static_cast<std::int16_t>(std::lrint(sample * 32767.0f));
  }

  alBufferData(buffer_id, AL_FORMAT_STEREO16, pcm_buffer_.data(),
               static_cast<ALsizei>(pcm_buffer_.size() * sizeof(std::int16_t)),
               kOutputSampleRate);

  const ALenum error = alGetError();
  if (error != AL_NO_ERROR) {
    if (error_message != nullptr) {
      *error_message = BuildAlError("fill OpenAL buffer", error);
    }
    return false;
  }

  return true;
}

std::string OpenAlAudioOutput::BuildAlError(std::string_view operation,
                                            ALenum error) const {
  return "Failed to " + std::string(operation) + ": " + AlErrorName(error);
}

std::string OpenAlAudioOutput::BuildAlcError(std::string_view operation) const {
  if (device_ == nullptr) {
    return "Failed to " + std::string(operation) + ": no OpenAL device";
  }

  return "Failed to " + std::string(operation) + ": " +
         AlcErrorName(alcGetError(device_));
}
