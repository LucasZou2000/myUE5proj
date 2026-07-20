// Copyright Epic Games, Inc. All Rights Reserved.

#include "Network/MYPROJ2NetworkSettings.h"

UMYPROJ2NetworkSettings::UMYPROJ2NetworkSettings()
{
}

const UMYPROJ2NetworkSettings* UMYPROJ2NetworkSettings::Get()
{
	return GetDefault<UMYPROJ2NetworkSettings>();
}
