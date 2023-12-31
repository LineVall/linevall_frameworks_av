/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef ANDROID_MEDIA_EFFECTSCONFIG_H
#define ANDROID_MEDIA_EFFECTSCONFIG_H

/** @file Parses audio effects configuration file to C and C++ structure.
 * @see audio_effects_conf_V2_0.xsd for documentation on each structure
 */

#include <error/Result.h>
#include <system/audio_effect.h>

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace android {
namespace effectsConfig {

/** Default path of effect configuration file. Relative to DEFAULT_LOCATIONS. */
constexpr const char* DEFAULT_NAME = "audio_effects.xml";

/** Directories where the effect libraries will be search for. */
constexpr const char* LD_EFFECT_LIBRARY_PATH[] =
#ifdef __LP64__
        {"/odm/lib64/soundfx", "/vendor/lib64/soundfx", "/system/lib64/soundfx"};
#else
        {"/odm/lib/soundfx", "/vendor/lib/soundfx", "/system/lib/soundfx"};
#endif

struct Library {
    std::string name;
    std::string path;
};
using Libraries = std::vector<std::shared_ptr<const Library>>;

struct EffectImpl {
    //< Only valid as long as the associated library vector is unmodified
    std::shared_ptr<const Library> library;
    effect_uuid_t uuid;
};

struct Effect : public EffectImpl {
    std::string name;
    bool isProxy;
    std::shared_ptr<EffectImpl> libSw; //< Only valid if isProxy
    std::shared_ptr<EffectImpl> libHw; //< Only valid if isProxy
};

using Effects = std::vector<std::shared_ptr<const Effect>>;

template <class Type>
struct Stream {
    Type type;
    Effects effects;
};
using OutputStream = Stream<audio_stream_type_t>;
using InputStream = Stream<audio_source_t>;

struct DeviceEffects : Stream<audio_devices_t> {
    std::string address;
};

struct Processings {
    std::vector<InputStream> preprocess;
    std::vector<OutputStream> postprocess;
    std::vector<DeviceEffects> deviceprocess;
};

/** Parsed configuration.
 * Intended to be a transient structure only used for deserialization.
 * Note: Everything is copied in the configuration from the xml dom.
 *       If copies needed to be avoided due to performance issue,
 *       consider keeping a private handle on the xml dom and replace all strings by dom pointers.
 *       Or even better, use SAX parsing to avoid the allocations all together.
 */
struct Config : public Processings {
    float version;
    Libraries libraries;
    Effects effects;
};

/** Result of `parse(const char*)` */
struct ParsingResult {
    /** Parsed config, nullptr if the xml lib could not load the file */
    std::shared_ptr<const Config> parsedConfig;
    size_t nbSkippedElement; //< Number of skipped invalid library, effect or processing chain
    const std::string configPath; //< Path to the loaded configuration
};

/** Parses the provided effect configuration.
 * Parsing do not stop of first invalid element, but continues to the next.
 * @param[in] path of the configuration file do load
 *                 if nullptr, look for DEFAULT_NAME in DEFAULT_LOCATIONS.
 * @see ParsingResult::nbSkippedElement
 */
ParsingResult parse(const char* path = nullptr);

} // namespace effectsConfig
} // namespace android
#endif  // ANDROID_MEDIA_EFFECTSCONFIG_H
