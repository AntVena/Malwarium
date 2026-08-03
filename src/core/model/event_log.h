// event_log.h — the rolling Hacker-Log (STAT page 2).
//
// A small ring buffer of recent events. v1 is the closed 5-type vocabulary;
// the renderer maps each type to one of three glyphs (item / warn / combat).
// Rows carry pre-formatted terse text (fixed buffers — embedded-friendly, no
// std::string / heap per entry).
#pragma once

namespace mal {

// The closed v1 event vocabulary.
enum class LogEventType { ItemGained, ItemUsed, CareMistake, CombatWon, CombatLost };

struct LogEntry {
    LogEventType type;
    char text[28];
};

class EventLog {
public:
    static constexpr int kCapacity = 8;  // keep last 8; STAT shows the last 3

    // Append an event; oldest falls off once full.
    void push(LogEventType type, const char* text);

    int size() const { return count_; }
    // Newest-first indexing: at(0) is the most recent event.
    const LogEntry& at(int i) const;

private:
    LogEntry entries_[kCapacity];
    int count_ = 0;
    int head_ = 0;  // index of the next write slot
};

} // namespace mal
