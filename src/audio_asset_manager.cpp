#include "audio_asset_manager.hpp"

#include <sndfile.h>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace {

std::string BuildSndfileError(std::string_view path) {
  return "Failed to load " + std::string(path) + ": " + sf_strerror(nullptr);
}

}  // namespace

bool AudioAssetManager::LoadCatalog(const SoundCatalog& catalog,
                                    std::string& error_message) {
  for (const SoundDef& def : catalog.All()) {
    if (!LoadSound(def, error_message)) {
      return false;
    }
  }

  return true;
}

void AudioAssetManager::Clear() { buffers_.clear(); }

const SoundBuffer* AudioAssetManager::Find(SoundId id) const noexcept {
  const auto it = buffers_.find(id);
  if (it == buffers_.end()) {
    return nullptr;
  }

  return &it->second;
}

bool AudioAssetManager::LoadSound(const SoundDef& def,
                                  std::string& error_message) {
  SF_INFO info{};
  SNDFILE* file = sf_open(def.file_path.data(), SFM_READ, &info);
  if (file == nullptr) {
    error_message = BuildSndfileError(def.file_path);
    return false;
  }

  std::vector<float> source_samples(static_cast<std::size_t>(info.frames) *
                                    info.channels);
  const sf_count_t read_frames =
      sf_readf_float(file, source_samples.data(), info.frames);
  sf_close(file);

  if (read_frames != info.frames) {
    error_message = "Short read while loading " + std::string(def.file_path);
    return false;
  }

  buffers_.emplace(def.id, ResampleToInternalFormat(
                               source_samples, info.channels, info.samplerate));
  return true;
}

SoundBuffer AudioAssetManager::ResampleToInternalFormat(
    const std::vector<float>& source_samples, int source_channels,
    int source_sample_rate) const {
  const std::size_t source_frames =
      source_samples.size() / static_cast<std::size_t>(source_channels);
  const double ratio = static_cast<double>(kOutputSampleRate) /
                       static_cast<double>(source_sample_rate);
  const std::size_t output_frames = std::max<std::size_t>(
      1, static_cast<std::size_t>(
             std::llround(static_cast<double>(source_frames) * ratio)));

  SoundBuffer output;
  output.sample_rate = kOutputSampleRate;
  output.channels = kOutputChannels;
  output.samples.resize(output_frames * kOutputChannels);

  for (std::size_t frame = 0; frame < output_frames; ++frame) {
    const double source_position = static_cast<double>(frame) / ratio;
    const std::size_t left_index = std::min<std::size_t>(
        source_frames - 1, static_cast<std::size_t>(source_position));
    const std::size_t right_index = std::min(source_frames - 1, left_index + 1);
    const float interpolation =
        static_cast<float>(source_position - static_cast<double>(left_index));

    for (int channel = 0; channel < kOutputChannels; ++channel) {
      const int source_channel = std::min(channel, source_channels - 1);
      const float left_sample =
          source_samples[left_index *
                             static_cast<std::size_t>(source_channels) +
                         source_channel];
      const float right_sample =
          source_samples[right_index *
                             static_cast<std::size_t>(source_channels) +
                         source_channel];
      output.samples[frame * kOutputChannels + channel] =
          left_sample + (right_sample - left_sample) * interpolation;
    }
  }

  return output;
}
