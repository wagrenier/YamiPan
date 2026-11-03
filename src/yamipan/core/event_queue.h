// SDL events on their way from the main thread, which pumps them, to the render thread, where ImGui runs.
// SDL owns an event's strings only until the next pump, so the text is copied and the pointers nobody reads
// here (drop data, IME candidates) are cleared.
#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <utility>

#include <SDL3/SDL_events.h>

namespace yamipan
{

class EventQueue
{
public:
    void push(const SDL_Event &event)
    {
        Entry entry{event, {}};
        switch (event.type)
        {
        case SDL_EVENT_TEXT_INPUT:
            entry.text = event.text.text != nullptr ? event.text.text : "";
            break;
        case SDL_EVENT_TEXT_EDITING:
            entry.text = event.edit.text != nullptr ? event.edit.text : "";
            break;
        case SDL_EVENT_TEXT_EDITING_CANDIDATES:
            entry.event.edit_candidates.candidates = nullptr;
            entry.event.edit_candidates.num_candidates = 0;
            break;
        case SDL_EVENT_DROP_BEGIN:
        case SDL_EVENT_DROP_FILE:
        case SDL_EVENT_DROP_TEXT:
        case SDL_EVENT_DROP_COMPLETE:
        case SDL_EVENT_DROP_POSITION:
            entry.event.drop.source = nullptr;
            entry.event.drop.data = nullptr;
            break;
        case SDL_EVENT_CLIPBOARD_UPDATE:
            entry.event.clipboard.mime_types = nullptr;
            entry.event.clipboard.num_mime_types = 0;
            break;
        default:
            break;
        }

        std::scoped_lock lock(m_lock);
        m_entries.push_back(std::move(entry));
    }

    // Hands every waiting event to `consume`; a text event's strings hold for the call.
    template <class Consumer>
    void drain(Consumer &&consume)
    {
        std::deque<Entry> entries;
        {
            std::scoped_lock lock(m_lock);
            entries.swap(m_entries);
        }
        for (Entry &entry : entries)
        {
            if (entry.event.type == SDL_EVENT_TEXT_INPUT)
                entry.event.text.text = entry.text.c_str();
            else if (entry.event.type == SDL_EVENT_TEXT_EDITING)
                entry.event.edit.text = entry.text.c_str();
            consume(entry.event);
        }
    }

private:
    struct Entry
    {
        SDL_Event event;
        std::string text;
    };

    std::mutex m_lock;
    std::deque<Entry> m_entries;
};

} // namespace yamipan
