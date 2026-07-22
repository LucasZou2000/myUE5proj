#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Weapons/WeaponStatLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMYPROJ2WeaponStatAggregationTest,
	"MYPROJ2.M6.WeaponStats.Aggregation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMYPROJ2WeaponStatAggregationTest::RunTest(const FString& Parameters)
{
	FWeaponStatBlock Base;
	Base.Damage = 20.0f;
	Base.FireIntervalSeconds = 0.2f;
	Base.RangeCm = 5000.0f;
	Base.MagazineSize = 12;

	FWeaponStatModifier A;
	A.Additive.Damage = 5.0f;
	A.Multiplicative.RangeCm = 1.1f;
	FWeaponStatModifier B;
	B.Additive.MagazineSize = 3;
	B.Multiplicative.Damage = 1.2f;

	const FWeaponStatBlock Result = UWeaponStatLibrary::BuildStatsFromModifiers(Base, {A, B});
	TestEqual(TEXT("damage applies add then multiply"), Result.Damage, 30.0f);
	TestEqual(TEXT("range multiplier applies"), Result.RangeCm, 5500.0f);
	TestEqual(TEXT("magazine additive applies"), Result.MagazineSize, 15);

	const FWeaponStatBlock Repeated = UWeaponStatLibrary::BuildStatsFromModifiers(Result, {});
	TestEqual(TEXT("empty rebuild does not drift damage"), Repeated.Damage, Result.Damage);
	TestEqual(TEXT("empty rebuild does not drift magazine"), Repeated.MagazineSize, Result.MagazineSize);
	TArray<FWeaponStatModifier> Reversed;
	Reversed.Add(B);
	Reversed.Add(A);
	const FWeaponStatBlock Reordered = UWeaponStatLibrary::BuildStatsFromModifiers(Base, Reversed);
	TestEqual(TEXT("aggregation is order independent"), Reordered.Damage, Result.Damage);
	TestEqual(TEXT("aggregation order independent magazine"), Reordered.MagazineSize, Result.MagazineSize);
	return true;
}

#endif
