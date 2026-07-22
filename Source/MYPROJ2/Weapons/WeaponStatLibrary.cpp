#include "Weapons/WeaponStatLibrary.h"
#include "Weapons/WeaponPartDefinition.h"

namespace
{
	void ClampStats(FWeaponStatBlock& Stats)
	{
		Stats.Damage = FMath::Clamp(Stats.Damage, 1.0f, 1000.0f);
		Stats.FireIntervalSeconds = FMath::Clamp(Stats.FireIntervalSeconds, 0.05f, 10.0f);
		Stats.RangeCm = FMath::Clamp(Stats.RangeCm, 100.0f, 20000.0f);
		Stats.SpreadDegrees = FMath::Clamp(Stats.SpreadDegrees, 0.0f, 45.0f);
		Stats.NoiseRadiusCm = FMath::Clamp(Stats.NoiseRadiusCm, 0.0f, 10000.0f);
		Stats.MagazineSize = FMath::Clamp(FMath::RoundToInt(static_cast<float>(Stats.MagazineSize)), 1, 500);
	}
}

FWeaponStatBlock UWeaponStatLibrary::BuildStats(const FWeaponStatBlock& BaseStats,
	const TArray<UWeaponPartDefinition*>& Parts)
{
	TArray<UWeaponPartDefinition*> SortedParts = Parts;
	SortedParts.Sort([](const UWeaponPartDefinition& A, const UWeaponPartDefinition& B)
	{
		return static_cast<uint8>(A.Slot) < static_cast<uint8>(B.Slot);
	});

	TArray<FWeaponStatModifier> Modifiers;
	Modifiers.Reserve(SortedParts.Num());
	for (const UWeaponPartDefinition* Part : SortedParts)
	{
		if (Part)
		{
			Modifiers.Add(Part->Modifier);
		}
	}
	return BuildStatsFromModifiers(BaseStats, Modifiers);
}

FWeaponStatBlock UWeaponStatLibrary::BuildStatsFromModifiers(const FWeaponStatBlock& BaseStats,
	const TArray<FWeaponStatModifier>& Modifiers)
{
	FWeaponStatBlock Result = BaseStats;
	FWeaponStatDelta Add;
	FWeaponStatScale Scale;
	for (const FWeaponStatModifier& Modifier : Modifiers)
	{
		Add.Damage += Modifier.Additive.Damage;
		Add.FireIntervalSeconds += Modifier.Additive.FireIntervalSeconds;
		Add.RangeCm += Modifier.Additive.RangeCm;
		Add.SpreadDegrees += Modifier.Additive.SpreadDegrees;
		Add.NoiseRadiusCm += Modifier.Additive.NoiseRadiusCm;
		Add.MagazineSize += Modifier.Additive.MagazineSize;
		Scale.Damage *= Modifier.Multiplicative.Damage;
		Scale.FireIntervalSeconds *= Modifier.Multiplicative.FireIntervalSeconds;
		Scale.RangeCm *= Modifier.Multiplicative.RangeCm;
		Scale.SpreadDegrees *= Modifier.Multiplicative.SpreadDegrees;
		Scale.NoiseRadiusCm *= Modifier.Multiplicative.NoiseRadiusCm;
		Scale.MagazineSize *= Modifier.Multiplicative.MagazineSize;
	}
	Result.Damage = (Result.Damage + Add.Damage) * Scale.Damage;
	Result.FireIntervalSeconds = (Result.FireIntervalSeconds + Add.FireIntervalSeconds) * Scale.FireIntervalSeconds;
	Result.RangeCm = (Result.RangeCm + Add.RangeCm) * Scale.RangeCm;
	Result.SpreadDegrees = (Result.SpreadDegrees + Add.SpreadDegrees) * Scale.SpreadDegrees;
	Result.NoiseRadiusCm = (Result.NoiseRadiusCm + Add.NoiseRadiusCm) * Scale.NoiseRadiusCm;
	Result.MagazineSize = FMath::RoundToInt((Result.MagazineSize + Add.MagazineSize) * Scale.MagazineSize);
	ClampStats(Result);
	return Result;
}
