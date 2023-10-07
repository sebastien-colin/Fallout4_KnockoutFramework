#pragma once
#include "f4se/GameData.h"
#include "f4se/GameReferences.h"
#include "f4se/GameSettings.h"

// DamageFrame - credit: kassent (https://github.com/kassent/FloatingDamage)
class BGSAttackData;
struct DamageFrame
{
	NiPoint3				hitLocation;				// 00
	UInt32					pad0C;						// 0C
	float					unk10[8];					// 10 - 2 vector[4]s: [0]=direction, [1]=direction for projectiles, distance or velocity for beams?
	bhkNPCollisionObject	* collisionObj;				// 30
	UInt64					unk38;						// 38
	UInt32					attackerHandle;				// 40
	UInt32					victimHandle;				// 44
	UInt64					unk48[1];					// 48 - filled with attackerHandle if source is null + actual dmg source is a creature weapon
	BGSAttackData			* attackData;				// 50 - filled if source is a melee/unarmed weapon, creature attack, or bash with a ranged weapon
	TESForm					* damageSourceForm;			// 58 - source weapon form
	TBO_InstanceData		* instanceData;				// 60 - source weapon instance
	UInt64					unk68[3];					// 68
	TESAmmo					* ammo;						// 80 - null for melee weapons and gun bash attacks
	void					* unk88;					// 88
	float					damage2;					// 90 - final damage
	float					unk94;						// 94 - damage before resistances
	float					damage;						// 98 - unk94 * distanceMult - mitigatedDmg (or 0 if beam projectile)
};
STATIC_ASSERT(sizeof(DamageFrame) == 0xA0);

struct ModMiscForms_Struct {
	ActorValueInfo * Health;
	BGSPerk * KFIsVictimKoEligiblePerk;
	BGSPerk * KFIsAttackerKoEligiblePerk;
};
extern ModMiscForms_Struct ModMiscForms;

struct ModKeywords_Struct {
	BGSKeyword * WeaponTypeUnarmed;
	BGSKeyword * QuickkeyMelee;
	BGSKeyword * AnimsBayonet;
	BGSKeyword * ActorTypeNPC;
	BGSKeyword * ActorTypeSuperMutant;
	BGSKeyword * ActorTypeFeralGhoul;
	BGSKeyword * KFKnockedOutKeyword;
	BGSKeyword * KFKnockoutTriggerKeyword;
	BGSKeyword * KFWeaponCanKnockoutKeyword;
	BGSKeyword * KFActorCantBeKnockedOutKeyword;
	BGSKeyword * KFActorCantKnockoutKeyword;
};
extern ModKeywords_Struct ModKeywords;

struct ModGlobals_Struct {
	TESGlobal * KFUnarmedEnabled;
	TESGlobal * KFBashEnabled;
	TESGlobal * KFCanKoPlayer;
	TESGlobal * KFCanKoHumans;
	TESGlobal * KFCanKoSuperMutants;
	TESGlobal * KFCanKoFeralGhoul;
	TESGlobal * KFCanKoOthers;
	TESGlobal * KFCanBeKoPlayer;
	TESGlobal * KFCanBeKoFollowers;
	TESGlobal * KFCanBeKoHumans;
	TESGlobal * KFCanBeKoSuperMutants;
	TESGlobal * KFCanBeKoFeralGhoul;
	TESGlobal * KFCanBeKoOthers;
};
extern ModGlobals_Struct ModGlobals;

/** native HasKeyword/GetVirtualFunction
		credit: shavkacagarikia (https://github.com/shavkacagarikia/ExtraItemInfo) **/
typedef bool(*_IKeywordFormBase_HasKeyword)(IKeywordFormBase* keywordFormBase, BGSKeyword* keyword, UInt32 unk3);

template <typename T>
T GetVirtualFunction(void* baseObject, int vtblIndex) {
	uintptr_t* vtbl = reinterpret_cast<uintptr_t**>(baseObject)[0];
	return reinterpret_cast<T>(vtbl[vtblIndex]);
}

namespace KnockoutFramework
{
	bool HasKeyword_Native(IKeywordFormBase * keywordBase, BGSKeyword * checkKW);

	bool IsAttackKoEligible(TESObjectREFR * attacker, TESObjectREFR * victim, TESObjectWEAP * weaponForm, TESObjectWEAP::InstanceData * weaponInstance, UInt32 attackType);
	
	bool IsVictimKoEligible(TESObjectREFR * victim, bool isPlayer = false);

	bool IsAttackerKoEligible(TESObjectREFR * attacker, bool isPlayer = false);

	float GetDamagesMult(bool isPlayer = false);
	
	DamageFrame * CancelDamages(DamageFrame * pDamageFrame, bool noDamages = false);
}