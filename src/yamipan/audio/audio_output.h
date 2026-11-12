// The sound card, as one SDL audio stream on the default playback device: 16-bit stereo at the rate the
// caller mixes at. The caller pushes blocks and paces itself on what is still queued.
#pragma once

#include <memory>
#include <string>

struct SDL_AudioStream;

namespace yamipan::audio
{

class AudioOutput
{
public:
    // nullptr when there is no device (or SDL's audio is not up): the caller runs silent.
    static std::unique_ptr<AudioOutput> open(int sampleRate);

    ~AudioOutput();
    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    // Bytes pushed and not yet played.
    int queuedBytes() const;
    void put(const void *frames, int bytes);
    const std::string &deviceName() const { return m_deviceName; }

private:
    AudioOutput(SDL_AudioStream *stream, std::string deviceName);

    SDL_AudioStream *m_stream;
    std::string m_deviceName;
};

} // namespace yamipan::audio
