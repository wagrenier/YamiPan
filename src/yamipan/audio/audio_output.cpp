#include "yamipan/audio/audio_output.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>

#include <utility>

#include "yamipan/core/logger.h"

namespace yamipan::audio
{

std::unique_ptr<AudioOutput> AudioOutput::open(int sampleRate)
{
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = sampleRate;

    SDL_AudioStream *stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (stream == nullptr)
    {
        log::warning("audio: no playback device ({}); the sound chip runs silent", SDL_GetError());
        return nullptr;
    }
    const char *name = SDL_GetAudioDeviceName(SDL_GetAudioStreamDevice(stream));
    SDL_ResumeAudioStreamDevice(stream);
    return std::unique_ptr<AudioOutput>(new AudioOutput(stream, name != nullptr ? name : "default"));
}

AudioOutput::AudioOutput(SDL_AudioStream *stream, std::string deviceName)
    : m_stream(stream), m_deviceName(std::move(deviceName))
{
}

AudioOutput::~AudioOutput()
{
    SDL_DestroyAudioStream(m_stream);
}

int AudioOutput::queuedBytes() const
{
    return SDL_GetAudioStreamQueued(m_stream);
}

void AudioOutput::put(const void *frames, int bytes)
{
    SDL_PutAudioStreamData(m_stream, frames, bytes);
}

} // namespace yamipan::audio
