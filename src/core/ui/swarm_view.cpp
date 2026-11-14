#include "core/ui/swarm_view.h"

namespace mal {

SwarmView swarmViewOf(const Flock& f) {
    SwarmView v;
    v.n = f.count() < kSwarmMarksMax ? f.count() : kSwarmMarksMax;
    for (int i = 0; i < v.n; ++i) {
        v.marks[i].x = static_cast<int16_t>(f.x(i));
        v.marks[i].y = static_cast<int16_t>(f.y(i));
        v.marks[i].vx = static_cast<int16_t>(f.vx(i));
        v.marks[i].vy = static_cast<int16_t>(f.vy(i));
    }
    v.cx = f.centreX();
    v.cy = f.centreY();
    v.spread = f.spread();
    return v;
}

}  // namespace mal
