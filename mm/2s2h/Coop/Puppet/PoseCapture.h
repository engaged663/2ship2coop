#pragma once
// Sends the local Link's pose to the server once per game frame (see coop/common/PlayerState.h).

// True while a real save is being played (not title screen, not file select).
bool PoseCapture_InGameplay();

// Called at the end of every game frame (after animations are final).
void PoseCapture_Tick();
