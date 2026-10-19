#include "common.h"
#include "Pad.h"
#include "ControllerConfig.h"

namespace { unsigned heldKeys; }
extern "C" void revcSetKeys(unsigned keys) { heldKeys = keys; }

void CapturePad(RwInt32 padID)
{
    if (padID != 0) return;
    CControllerState &state = CPad::GetPad(0)->PCTempJoyState;
    state.Clear();
    // libnds key bits; feed a normal reVC controller state.
    state.Cross = (heldKeys & (1u << 0)) ? 255 : 0;
    state.Circle = (heldKeys & (1u << 1)) ? 255 : 0;
    state.Select = (heldKeys & (1u << 2)) ? 255 : 0;
    state.Start = (heldKeys & (1u << 3)) ? 255 : 0;
    state.DPadRight = (heldKeys & (1u << 4)) ? 255 : 0;
    state.DPadLeft = (heldKeys & (1u << 5)) ? 255 : 0;
    state.DPadUp = (heldKeys & (1u << 6)) ? 255 : 0;
    state.DPadDown = (heldKeys & (1u << 7)) ? 255 : 0;
    state.RightShoulder1 = (heldKeys & (1u << 8)) ? 255 : 0;
    state.LeftShoulder1 = (heldKeys & (1u << 9)) ? 255 : 0;
    state.Triangle = (heldKeys & (1u << 10)) ? 255 : 0;
    state.Square = (heldKeys & (1u << 11)) ? 255 : 0;
    state.LeftStickX = ((state.DPadRight != 0) - (state.DPadLeft != 0)) * 128;
    state.LeftStickY = ((state.DPadDown != 0) - (state.DPadUp != 0)) * 128;
    ControlsManager.m_OldState = ControlsManager.m_NewState;
    ControlsManager.m_NewState.numButtons = 12;
    for (int i = 0; i < 12; i++)
        ControlsManager.m_NewState.buttons[i] = (heldKeys >> i) & 1u;
}
