// content_crews.cpp — the crew table. One row per enlistable crew; see
// content_crews.h for the field schema and how a new Exploit kind is added.

#include "core/content/content_crews.h"

#include <cstring>

namespace mal {

const CrewDef kCrews[] = {
    // The first Blue crew, open to every operator from the start (no unlock beyond
    // designating a home network). Its Exploit is the crew's whole personality: three
    // incoming attacks bounce off outright before real damage resumes.
    {"deniers_of_service", "DENIERS OF SERVICE", "cannot touch: permission denied",
     CrewTeam::Blue, {"DENIAL OF SERVICE", CrewExploitKind::NegateNextHits, 3}},
};
const int kCrewCount = static_cast<int>(sizeof(kCrews) / sizeof(kCrews[0]));

const char* crewExploitTag(CrewExploitKind kind) {
    switch (kind) {
        case CrewExploitKind::NegateNextHits: return "NEGATE";
        case CrewExploitKind::None:           break;
    }
    return "";
}

const CrewDef* crew(int index) {
    return (index >= 0 && index < kCrewCount) ? &kCrews[index] : nullptr;
}

int crewIndexById(const char* id) {
    if (!id || id[0] == '\0') return -1;
    for (int i = 0; i < kCrewCount; ++i)
        if (std::strcmp(kCrews[i].id, id) == 0) return i;
    return -1;
}

}  // namespace mal
