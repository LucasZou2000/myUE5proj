#pragma once

#include "Engine/EngineTypes.h"

/**
 * Project trace channels. These must match the entries declared in
 * Config/DefaultEngine.ini under [/Script/Engine.CollisionProfile]:
 *
 *   +DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,...Name="WeaponTrace")
 *   +DefaultChannelResponses=(Channel=ECC_GameTraceChannel2,...Name="InteractionTrace")
 *
 * M3 splits combat and interaction off ECC_Visibility so that containers and
 * other UI-targetable world actors can configure independent responses without
 * colliding with camera/visibility traces.
 */
#define MYPROJ2_TRACE_CHANNEL_WEAPON       ECC_GameTraceChannel1
#define MYPROJ2_TRACE_CHANNEL_INTERACTION  ECC_GameTraceChannel2
