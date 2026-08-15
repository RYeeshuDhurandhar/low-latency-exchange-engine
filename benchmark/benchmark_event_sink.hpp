#pragma once

#include <cstdint>

#include "event.hpp"

struct BenchmarkEventSink {
    std::uint64_t trades = 0;
    std::uint64_t accepted = 0;
    std::uint64_t rejected = 0;
    std::uint64_t cancelled = 0;
    std::uint64_t modified = 0;
    std::uint64_t rested = 0;
    std::uint64_t book_updated = 0;
    std::uint64_t total_events = 0;

    void reset() {
        trades = 0;
        accepted = 0;
        rejected = 0;
        cancelled = 0;
        modified = 0;
        rested = 0;
        book_updated = 0;
        total_events = 0;
    }

    void on_event(const Event& event) {
        ++total_events;

        switch(event.event_type) {
            case EventType::Trade:
                trades++;
                break;

            case EventType::OrderAccepted:
                accepted++;
                break;

            case EventType::OrderRejected:
                rejected++;
                break;

            case EventType::OrderCancelled:
                cancelled++;
                break;

            case EventType::OrderModified:
                modified++;
                break;

            case EventType::OrderRested:
                rested++;
                break;

            case EventType::BookUpdated:
                book_updated++;
                break;

            default:
                break;
        }
    }
};
