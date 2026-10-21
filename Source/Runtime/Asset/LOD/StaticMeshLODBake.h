#pragma once

#include "Core/Types.h"

struct FLODBakeHeader
{
    char Magic[4]{ 'S', 'L', 'O', 'D' };
    uint32 Version = 1;
    uint64 LOD0Hash = 0;
    uint32 LODCount = 3;
    float ScreenThresholds[3]{};
};

