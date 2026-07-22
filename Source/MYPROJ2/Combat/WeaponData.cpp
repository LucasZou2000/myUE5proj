#include "Combat/WeaponData.h"

UWeaponData::UWeaponData()
{
	ItemId = TEXT("TestRifle");
	ItemType = EItemType::Weapon;
	GridSize = FIntPoint(2, 4);
	MaxStack = 1;
}

FWeaponStatBlock UWeaponData::GetEffectiveBaseStats() const
{
	if (!bUseLegacyM2Stats)
	{
		return BaseStats;
	}

	FWeaponStatBlock LegacyStats = BaseStats;
	LegacyStats.Damage = Damage;
	LegacyStats.FireIntervalSeconds = 60.0f / FMath::Max(1.0f, FireRate);
	LegacyStats.RangeCm = Range;
	LegacyStats.MagazineSize = FMath::Max(1, Ammo);
	return LegacyStats;
}
