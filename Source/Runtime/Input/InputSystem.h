#pragma once

#include "KeyCode.h"
#include "MouseButton.h"
#include "Core/Types.h"

enum class EGameInputAxis : uint8 { MouseX, MouseY, MouseWheel };

class FInputSystem
{
public:
    static bool IsKeyPressed(EKeyCode KeyCode);
    static bool IsKeyReleased(EKeyCode KeyCode);
    static bool IsKeyDown(EKeyCode KeyCode);

    static bool IsMouseDown(EMouseButton MouseButton);
    static bool IsMousePressed(EMouseButton MouseButton);
    static bool IsMouseReleased(EMouseButton MouseButton);

    static void UpdateInputStates();
    static void ClearAllStates();
    static void SyncMousePosition(int32 x, int32 y);
    static void OnKeyUp(int VirtualKey);
    static void OnMouseUp(EMouseButton MouseButton);
    static void OnKeyDown(int VirtualKey);
    static void OnMouseDown(EMouseButton MouseButton);
    static void OnMouseMove(int32 x, int32 y);
    static void OnMouseWheelDelta(int32 Wheel);

    static inline int32 GetMouseX() { return MouseX; }
    static inline int32 GetMouseY() { return MouseY; }

    static inline int32 GetMouseDeltaX() { return DeltaX; }
    static inline int32 GetMouseDeltaY() { return DeltaY; }

    static inline int32 GetWheelDelta() 
    { 
        int32 Delta = WheelDelta;
        WheelDelta = 0;
        return Delta;
    }

private:
    FInputSystem() = delete;

    static inline bool bMouseStates[5] = {};
    static inline bool bPrevMouseStates[5] = {};

    static inline bool bKeyStates[256] = {};
    static inline bool bPrevKeyStates[256] = {};

    static inline int32 MouseX = 0, MouseY = 0;
    static inline int32 PrevMouseX = 0, PrevMouseY = 0;
    static inline int32 DeltaX = 0, DeltaY = 0;

    static inline int32 WheelDelta = 0;
};
