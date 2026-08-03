#include "core/model/event_log.h"

#include <cstring>

namespace mal {

void EventLog::push(LogEventType type, const char* text) {
    LogEntry& e = entries_[head_];
    e.type = type;
    std::strncpy(e.text, text ? text : "", sizeof(e.text) - 1);
    e.text[sizeof(e.text) - 1] = '\0';
    head_ = (head_ + 1) % kCapacity;
    if (count_ < kCapacity) ++count_;
}

const LogEntry& EventLog::at(int i) const {
    // i=0 is newest: walk back from the most recent write.
    int idx = (head_ - 1 - i % kCapacity + 2 * kCapacity) % kCapacity;
    return entries_[idx];
}

} // namespace mal
