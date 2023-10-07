#include "f4se_common/F4SE_version.h"
#include "f4se_common/BranchTrampoline.h"
#include "f4se/PapyrusEvents.h"

#include <shlobj.h>

#include "KnockoutFramework.h"

#define PLUGIN_VERSION_MAJOR	1
#define PLUGIN_VERSION_MINOR	4
#define PLUGIN_VERSION_BUILD	0

#define PLUGIN_NAME		"Knockout Framework"
#define FILE_NAME		"KnockoutFramework"
#define BGS_PLUGIN_NAME	(std::string)"Knockout Framework.esm"
#define PLUGIN_VERSION	((PLUGIN_VERSION_MAJOR * 10000) + (PLUGIN_VERSION_MINOR * 100) + PLUGIN_VERSION_BUILD)

IDebugLog						gLog;
PluginHandle					g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface			* g_messaging = nullptr;
F4SEPapyrusInterface			* papyrusInterface = nullptr;

/** Actor::ProcessDamageFrame
sig: 48 8B C4 48 89 50 10 55 56 41 56 41 57
address:
v1.10.163: 0xE01630
credit: kassent (https://github.com/kassent/FloatingDamage) */
using _Process = void(*)(void *, DamageFrame *);
RelocAddr<_Process>	ProcessDamageFrame = 0xE01630;

ModGlobals_Struct ModGlobals;
ModKeywords_Struct ModKeywords;
ModMiscForms_Struct ModMiscForms;

namespace Main {
	DamageFrame * SetKnockoutStatus(DamageFrame * pDamageFrame) {
		if (!pDamageFrame || pDamageFrame->unk94 == 0.0) return pDamageFrame;

		NiPointer<TESObjectREFR> victim = nullptr;
		NiPointer<TESObjectREFR> attacker = nullptr;

		if (pDamageFrame == nullptr
			|| (LookupREFRByHandle((UInt32)pDamageFrame->victimHandle, victim), victim == nullptr) \
			|| (LookupREFRByHandle((UInt32)pDamageFrame->attackerHandle, attacker), attacker == nullptr) \
			|| victim->formType != FormType::kFormType_ACHR || attacker->formType != FormType::kFormType_ACHR) {
			return pDamageFrame;
		}

		TESObjectWEAP * weaponForm = nullptr;
		TESObjectWEAP::InstanceData * weaponInstance = nullptr;

		if (pDamageFrame->damageSourceForm) {
			if (pDamageFrame->damageSourceForm->formType == kFormType_WEAP) {
				weaponForm = reinterpret_cast<TESObjectWEAP*>(pDamageFrame->damageSourceForm);
			}
		}

		if (pDamageFrame->instanceData) {
			weaponInstance = reinterpret_cast<TESObjectWEAP::InstanceData*>(pDamageFrame->instanceData);
		} else if (weaponForm) {
			weaponInstance = &weaponForm->weapData;
		}

		UInt32 attackType = 0; // Ranged attack
		if (pDamageFrame->attackData) {
			if (pDamageFrame->damageSourceForm) {
				TESObjectWEAP * tempWeap = reinterpret_cast<TESObjectWEAP*>(pDamageFrame->damageSourceForm);
				if (tempWeap && tempWeap->weapData.ammo) attackType = 3; // Gun Bash
				else attackType = 1; // Melee weapon
			} else attackType = 2; // Melee attack
		}

		if (!KnockoutFramework::IsAttackKoEligible(attacker, victim, weaponForm, weaponInstance, attackType)) return pDamageFrame;
		else {
			float damagesMult = KnockoutFramework::GetDamagesMult(victim->formID == 0x14);
			//_DMESSAGE("INFO: DamageFrame (BASE) | unk94 : %f | damage : %f | damage2 : %f", pDamageFrame->unk94, pDamageFrame->damage, pDamageFrame->damage2);
			//_DMESSAGE("INFO: DamageFrame (MULT) | unk94 : %f | damage : %f | damage2 : %f", pDamageFrame->unk94 * damagesMult, pDamageFrame->damage * damagesMult, pDamageFrame->damage2 * damagesMult);

			bool victim_alive = ((victim->actorValueOwner.GetValue(ModMiscForms.Health) - (pDamageFrame->damage2 * damagesMult)) > 0.0f ? true : false);
			if (!victim_alive) {
				if (!KnockoutFramework::IsVictimKoEligible(victim, (victim->formID == 0x14)) \
					|| !KnockoutFramework::IsAttackerKoEligible(attacker, (attacker->formID == 0x14))) {
					return pDamageFrame;
				}
				if (KnockoutFramework::HasKeyword_Native(&victim->keywordFormBase, ModKeywords.KFKnockoutTriggerKeyword) \
					|| KnockoutFramework::HasKeyword_Native(&victim->keywordFormBase, ModKeywords.KFKnockedOutKeyword)) {
					return KnockoutFramework::CancelDamages(pDamageFrame);
				}

				//_DMESSAGE("INFO: %s triggered Knockout event on %s because his theorical health reached %.4f.",
				//	attacker->baseForm->GetFullName(), victim->baseForm->GetFullName(),
				//	victim->actorValueOwner.GetValue(ModMiscForms.Health) - (pDamageFrame->damage2 * damagesMult));

				struct KoEventData_Struct {
					Actor * akVictim;
					Actor * akAttacker;
				} KoEventData;

				KoEventData.akVictim = reinterpret_cast<Actor*>((TESObjectREFR*)victim);
				KoEventData.akAttacker = reinterpret_cast<Actor*>((TESObjectREFR*)attacker);

				papyrusInterface->GetExternalEventRegistrations("TriggerKoEvent", &KoEventData, [](UInt64 handle, const char * scriptName, const char * callbackName, void * dataPtr) {
					KoEventData_Struct * KoEventData = static_cast<KoEventData_Struct*>(dataPtr);
					SendPapyrusEvent2<Actor*, Actor*>(handle, scriptName, callbackName, KoEventData->akVictim, KoEventData->akAttacker);
				});

				return KnockoutFramework::CancelDamages(pDamageFrame);
			}
		}
		
		return pDamageFrame;
	}
};

SimpleLock globalDamageLock;
class ActorEx : public Actor {
public:
	static void ProcessDamageFrame_Hook(Actor * pObj, DamageFrame * pDamageFrame) {
		globalDamageLock.Lock();
		pDamageFrame = Main::SetKnockoutStatus(pDamageFrame);
		ProcessDamageFrame(pObj, pDamageFrame);
		globalDamageLock.Release();
	}
};

namespace Settings {
	TESForm * GetFormFromIdentifier(const std::string & formIdentifier) {
		UInt32 formId = 0;
		if (formIdentifier.c_str() != "none") {
			std::size_t pos = formIdentifier.find_first_of("|");
			std::string modName = formIdentifier.substr(0, pos);
			std::string modForm = formIdentifier.substr(pos + 1);
			sscanf_s(modForm.c_str(), "%X", &formId);
			if (formId != 0x0) {
				UInt8 modIndex = (*g_dataHandler)->GetLoadedModIndex(modName.c_str());
				if (modIndex != 0xFF) {
					formId |= ((UInt32)modIndex) << 24;
				} else {
					UInt16 lightModIndex = (*g_dataHandler)->GetLoadedLightModIndex(modName.c_str());
					if (lightModIndex != 0xFFFF) {
						formId |= 0xFE000000 | (UInt32(lightModIndex) << 12);
					} else {
						_MESSAGE("FormID %s not found!", formIdentifier.c_str());
						formId = 0;
					}
				}
			}
		}
		return (formId != 0x0) ? LookupFormByID(formId) : nullptr;
	}

	static void DefineGameForms() {
		std::string	string_form = "";
		TESForm * form = nullptr;

		// Misc forms

		string_form = "Fallout4.esm|2D4";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == ActorValueInfo::kTypeID) ModMiscForms.Health = (ActorValueInfo *)form;
		else _FATALERROR("ERROR: The 'Health' (%s) actor value could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|FA0";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSPerk::kTypeID) ModMiscForms.KFIsVictimKoEligiblePerk = (BGSPerk*)form;
		else _FATALERROR("ERROR: The 'KFIsVictimKoEligiblePerk' (%s) perk could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|1ED9";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSPerk::kTypeID) ModMiscForms.KFIsAttackerKoEligiblePerk = (BGSPerk*)form;
		else _FATALERROR("ERROR: The 'KFIsAttackerKoEligiblePerk' (%s) perk could not be found", string_form.c_str());

		// Global Variables

		string_form = BGS_PLUGIN_NAME + "|726D";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFUnarmedEnabled = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFUnarmedEnabled' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|35A3";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFBashEnabled = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFBashEnabled' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AF6";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanKoPlayer = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanKoPlayer' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AF7";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanKoHumans = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanKoHumans' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AF8";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanKoSuperMutants = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanKoSuperMutants' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AF9";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanKoFeralGhoul = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanKoFeralGhoul' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AFA";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanKoOthers = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanKoOthers' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AFE";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanBeKoPlayer = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanBeKoPlayer' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|2E13";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanBeKoFollowers = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanBeKoFollowers' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AFC";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanBeKoHumans = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanBeKoHumans' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AFF";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanBeKoSuperMutants = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanBeKoSuperMutants' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AFB";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanBeKoFeralGhoul = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanBeKoFeralGhoul' (%s) global could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|6AFD";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == TESGlobal::kTypeID) ModGlobals.KFCanBeKoOthers = (TESGlobal*)form;
		else _FATALERROR("ERROR: The 'KFCanBeKoOthers' (%s) global could not be found", string_form.c_str());

		// Keywords

		string_form = "Fallout4.esm|5240E";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.WeaponTypeUnarmed = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'WeaponTypeUnarmed' (%s) keyword could not be found", string_form.c_str());

		string_form = "Fallout4.esm|10C89B";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.QuickkeyMelee = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'QuickkeyMelee' (%s) keyword could not be found", string_form.c_str());
		
		string_form = "Fallout4.esm|444F7";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.AnimsBayonet = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'AnimsBayonet' (%s) keyword could not be found", string_form.c_str());

		string_form = "Fallout4.esm|13794";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.ActorTypeNPC = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'ActorTypeNPC' (%s) keyword could not be found", string_form.c_str());

		string_form = "Fallout4.esm|6D7B6";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.ActorTypeSuperMutant = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'ActorTypeSuperMutant' (%s) keyword could not be found", string_form.c_str());

		string_form = "Fallout4.esm|6B4F2";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.ActorTypeFeralGhoul = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'ActorTypeFeralGhoul' (%s) keyword could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|1ED5";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.KFKnockedOutKeyword = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'KFKnockedOutKeyword' (%s) keyword could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|F413";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.KFKnockoutTriggerKeyword = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'KFKnockoutTriggerKeyword' (%s) keyword could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|FA2";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.KFWeaponCanKnockoutKeyword = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'KFWeaponCanKnockoutKeyword' (%s) keyword could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|AF8B";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.KFActorCantBeKnockedOutKeyword = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'KFActorCantBeKnockedOutKeyword' (%s) keyword could not be found", string_form.c_str());

		string_form = BGS_PLUGIN_NAME + "|AF8D";
		form = GetFormFromIdentifier(string_form);
		if (form && form->formType == BGSKeyword::kTypeID) ModKeywords.KFActorCantKnockoutKeyword = (BGSKeyword *)form;
		else _FATALERROR("ERROR: The 'KFActorCantKnockoutKeyword' (%s) keyword could not be found", string_form.c_str());
	}

	static void InitHooks() {
		g_branchTrampoline.Write5Call(RELOC_RUNTIME_ADDR("E8 ? ? ? ? 48 85 FF 74 36 48 8B CF"), (uintptr_t)ActorEx::ProcessDamageFrame_Hook);
	}

	static void MessageCallback(F4SEMessagingInterface::Message* msg) {
		switch (msg->type) {
		case (F4SEMessagingInterface::kMessage_GameDataReady):
			DefineGameForms();
			break;
		default:
			// No action
			break;
		}
	}
}

extern "C" {
	bool F4SEPlugin_Query(const F4SEInterface * f4se, PluginInfo * info) {
		std::unique_ptr<char[]> sPath(new char[MAX_PATH]);
		sprintf_s(sPath.get(), MAX_PATH, "%s%s.log", "\\My Games\\Fallout4\\F4SE\\", FILE_NAME);
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, sPath.get());

		_MESSAGE("%s library v%d.%d.%d - Loaded", PLUGIN_NAME, PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_BUILD);

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = FILE_NAME;
		info->version = PLUGIN_VERSION;

		g_pluginHandle = f4se->GetPluginHandle();
		plugin_info.plugin_name = FILE_NAME;
		plugin_info.runtime_version = f4se->runtimeVersion;

		if (f4se->isEditor) {
			_FATALERROR("WARNING: Plugin loaded in the editor, shutting down...");
			return false;
		}

		g_messaging = (F4SEMessagingInterface *)f4se->QueryInterface(kInterface_Messaging);
		if (!g_messaging) {
			_FATALERROR("ERROR: Couldn't get the messaging interface.");
			return false;
		}

		papyrusInterface = (F4SEPapyrusInterface*)f4se->QueryInterface(kInterface_Papyrus);
		if (!papyrusInterface) {
			_FATALERROR("ERROR: Couldn't get the papyrus interface.");
			return false;
		}

		return true;
	}

	bool F4SEPlugin_Load(const F4SEInterface * f4se) {
		if (!g_branchTrampoline.Create(1024 * 64)) {
			_FATALERROR("ERROR: The trampoline just experienced its last bounce. Wait for a mod update.");
			return false;
		}

		try {
			sig_scan_timer timer;
			Settings::InitHooks();
		} catch (const no_result_exception & exception) {
			_FATALERROR(exception.what());
			MessageBoxA(nullptr, "ERROR: Signature scan failed, please update Knockout Framework.", PLUGIN_NAME, MB_ICONASTERISK);
			return false;
		}

		if (g_messaging != nullptr) g_messaging->RegisterListener(g_pluginHandle, "F4SE", Settings::MessageCallback);

		return true;
	}
};