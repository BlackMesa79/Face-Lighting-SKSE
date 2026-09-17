#pragma once

namespace FaceLight {
    void Install();
    void SetGameActive(bool active);
    // UI/render callbacks queue work rather than touching the scene graph.
    void RequestUpdate();
}
