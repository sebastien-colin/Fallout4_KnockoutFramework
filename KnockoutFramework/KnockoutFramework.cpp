#include "KnockoutFramework.h"

namespace KnockoutFramework
{
	bool HasKeyword_Native(IKeywordFormBase * keywordBase, BGSKeyword * checkKW)
	{
		if (!checkKW || !keywordBase) {
			return false;
		}
		auto HasKeyword_Internal = GetVirtualFunction<_IKeywordFormBase_HasKeyword>(keywordBase, 1);
		return HasKeyword_Internal(keywordBase, checkKW, 0);
	}

	bool IsAttackKoEligible(TESObjectREFR * attacker, TESObjectREFR * victim, TESObjectWEAP * weaponForm, TESObjectWEAP::InstanceData * weaponInstance, UInt32 attackType)
	{
		if (weaponInstance && weaponInstance->keywords && weaponInstance->keywords->numKeywords > 0) {
			if (attackType == 3) {
				if (HasKeyword_Native(&weaponInstance->keywords->keywordBase, ModKeywords.AnimsBayonet)) return false; // Bayonet : Lethal
				else return ((int)ModGlobals.KFBashEnabled->value == 1);
			}
			if (HasKeyword_Native(&weaponInstance->keywords->keywordBase, ModKeywords.KFWeaponCanKnockoutKeyword)) {
				return true; // Certified non-lethal weapon
			}
			if ((attackType == 1 || attackType == 2) \
				&& HasKeyword_Native(&weaponInstance->keywords->keywordBase, ModKeywords.WeaponTypeUnarmed) \
				&& !HasKeyword_Native(&weaponInstance->keywords->keywordBase, ModKeywords.QuickkeyMelee)) {
				return true; // Unarmed attack
			}
		}
		if (weaponForm && &weaponForm->keyword) {
			if (attackType == 3) {
				if (HasKeyword_Native(&weaponForm->keyword.keywordBase, ModKeywords.AnimsBayonet)) return false; // Bayonet : Lethal
				else return ((int)ModGlobals.KFBashEnabled->value == 1);
			}
			if (HasKeyword_Native(&weaponForm->keyword.keywordBase, ModKeywords.KFWeaponCanKnockoutKeyword)) {
				return true; // Certified non-lethal weapon
			}
			if ((attackType == 1 || attackType == 2) \
				&& HasKeyword_Native(&weaponForm->keyword.keywordBase, ModKeywords.WeaponTypeUnarmed) \
				&& !HasKeyword_Native(&weaponForm->keyword.keywordBase, ModKeywords.QuickkeyMelee)) {
				return true; // Unarmed attack
			}
		}

		return false;
	}

	bool IsVictimKoEligible(TESObjectREFR * victim, bool isPlayer) {
		BGSPerk* conditionalPerk = ModMiscForms.KFIsVictimKoEligiblePerk;
		Condition ** condition = &conditionalPerk->condition;
		if (condition) {
			if (!EvaluationConditions(condition, victim, victim)) return false;
		} else return false;

		if (HasKeyword_Native(&victim->keywordFormBase, ModKeywords.KFActorCantBeKnockedOutKeyword)) return false;
		else if (isPlayer) {
			if ((int)ModGlobals.KFCanBeKoPlayer->value == 0) return false;
		} else if (reinterpret_cast<Actor*>(victim)->IsPlayerTeammate()) {
			if ((int)ModGlobals.KFCanBeKoFollowers->value == 0) return false;
		} else if (HasKeyword_Native(&victim->keywordFormBase, ModKeywords.ActorTypeNPC)) {
			if ((int)ModGlobals.KFCanBeKoHumans->value == 0) return false;
		} else if (HasKeyword_Native(&victim->keywordFormBase, ModKeywords.ActorTypeSuperMutant)) {
			if ((int)ModGlobals.KFCanBeKoSuperMutants->value == 0) return false;
		} else if (HasKeyword_Native(&victim->keywordFormBase, ModKeywords.ActorTypeFeralGhoul)) {
			if ((int)ModGlobals.KFCanBeKoFeralGhoul->value == 0) return false;
		} else if ((int)ModGlobals.KFCanBeKoOthers->value == 0) return false;
		
		return true;
	}

	bool IsAttackerKoEligible(TESObjectREFR * attacker, bool isPlayer) {
		BGSPerk* conditionalPerk = ModMiscForms.KFIsAttackerKoEligiblePerk;
		Condition ** condition = &conditionalPerk->condition;
		if (condition) {
			if (!EvaluationConditions(condition, attacker, attacker)) return false;
		} else return false;
		
		if (HasKeyword_Native(&attacker->keywordFormBase, ModKeywords.KFActorCantKnockoutKeyword)) return false;
		else if (isPlayer) {
			if ((int)ModGlobals.KFCanKoPlayer->value == 0) return false;
		} else if (HasKeyword_Native(&attacker->keywordFormBase, ModKeywords.ActorTypeNPC)) {
			if ((int)ModGlobals.KFCanKoHumans->value == 0) return false;
		} else if (HasKeyword_Native(&attacker->keywordFormBase, ModKeywords.ActorTypeSuperMutant)) {
			if ((int)ModGlobals.KFCanKoSuperMutants->value == 0) return false;
		} else if (HasKeyword_Native(&attacker->keywordFormBase, ModKeywords.ActorTypeFeralGhoul)) {
			if ((int)ModGlobals.KFCanKoFeralGhoul->value == 0) return false;
		} else if ((int)ModGlobals.KFCanKoOthers->value == 0) return false;

		return true;
	}

	float GetDamagesMult(bool isPlayer) {
		float damagesMultDefault = 1.0;

		Setting	* difficulty_setting = GetINISetting("iDifficulty:Gameplay");
		if (!difficulty_setting) {
			_ERROR("ERROR: The iDifficulty setting of the [Gameplay] section could not be found.");
			return damagesMultDefault;
		}

		char * diff_mult_string = nullptr;
		switch (difficulty_setting->data.u32) {
		case 0:
			if (isPlayer) diff_mult_string = "fDiffMultHPToPCVE";
			else diff_mult_string = "fDiffMultHPByPCVE";
			break;
		case 1:
			if (isPlayer) diff_mult_string = "fDiffMultHPToPCE";
			else diff_mult_string = "fDiffMultHPByPCE";
			break;
		case 2:
			if (isPlayer) diff_mult_string = "fDiffMultHPToPCN";
			else diff_mult_string = "fDiffMultHPByPCN";
			break;
		case 3:
			if (isPlayer) diff_mult_string = "fDiffMultHPToPCH";
			else diff_mult_string = "fDiffMultHPByPCH";
			break;
		case 4:
			if (isPlayer) diff_mult_string = "fDiffMultHPToPCVH";
			else diff_mult_string = "fDiffMultHPByPCVH";
			break;
		case 6:
			if (isPlayer) diff_mult_string = "fDiffMultHPToPCSV";
			else diff_mult_string = "fDiffMultHPByPCSV";
			break;
		default:
			_ERROR("ERROR: Unknown game difficulty : %d", difficulty_setting->data.u32);
			return damagesMultDefault;
			break;
		}

		Setting	* diff_mult_value = GetGameSetting(diff_mult_string);
		if (!diff_mult_value) {
			_ERROR("ERROR: The '%s' setting could not be found in the game setings.", diff_mult_string);
			return damagesMultDefault;
		}

		return (diff_mult_value->data.f32 ? diff_mult_value->data.f32 : damagesMultDefault);
	}

	DamageFrame * CancelDamages(DamageFrame * pDamageFrame, bool noDamages)
	{
		float final_damage = (noDamages ? 0.0 : 0.000001);
		pDamageFrame->damage2 = final_damage;
		pDamageFrame->damage = final_damage;
		pDamageFrame->unk94 = final_damage;
		return pDamageFrame;
	}
}