#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "sound_catalog.hpp"

/**
 * @brief Parses a YAML/config sound id key into a SoundId value.
 *
 * @param id External YAML sound id key.
 * @param sound_id Receives the parsed sound id on success.
 * @return true when the key is recognized.
 */
[[nodiscard]] bool TryParseSoundId(std::string_view id, SoundId* sound_id);

/**
 * @brief Parses a YAML/config event id key into an EventId value.
 *
 * @param id External YAML event id key.
 * @param event_id Receives the parsed event id on success.
 * @return true when the key is recognized.
 */
[[nodiscard]] bool TryParseEventId(std::string_view id, EventId* event_id);

/**
 * @brief Runtime view of the YAML-backed sound catalog.
 */
class SoundCatalog {
 public:
  /**
   * @brief Loads catalog definitions from a YAML file.
   *
   * @param path YAML catalog path.
   * @param error_message Receives a human-readable error on failure.
   * @return true when the catalog was loaded and validated.
   */
  bool LoadFromFile(std::string_view path, std::string& error_message);

  /**
   * @brief Returns all catalog definitions.
   */
  [[nodiscard]] std::span<const SoundDef> All() const noexcept;

  /**
   * @brief Returns all event mappings.
   */
  [[nodiscard]] std::span<const EventDef> Events() const noexcept;

  /**
   * @brief Finds one catalog definition by sound id.
   *
   * @return Pointer to the definition, or nullptr if the id is unknown.
   */
  [[nodiscard]] const SoundDef* Find(SoundId id) const noexcept;

  /**
   * @brief Finds one event definition by event id.
   *
   * @return Pointer to the event definition, or nullptr if the id is unknown.
   */
  [[nodiscard]] const EventDef* Find(EventId id) const noexcept;

  /**
   * @brief Resolves an event id to its configured sound actions.
   *
   * @return Span of actions, or an empty span if the event is unknown.
   */
  [[nodiscard]] std::span<const SoundAction> ResolveEventActions(
      EventId id) const noexcept;

 private:
  std::vector<SoundDef> definitions_;
  std::vector<EventDef> events_;
};
