// CULT-ULHU character data asset: Data Asset -> core CharacterDef.
#include "Data/CultCharacterData.h"

namespace
{
	std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
}

cultulhu::SpellDef FCultSpellData::ToCoreDef() const
{
	cultulhu::SpellDef D;
	D.id = ToStd(SpellId);
	D.name = ToStd(Name);
	D.flavor = ToStd(Flavor);
	D.cooldownSec = CooldownSec;
	D.staminaCost = StaminaCost;
	D.manaCost = ManaCost;
	D.effectKind = ToStd(EffectKind);
	D.effectPower = EffectPower;
	D.range = Range;
	return D;
}

cultulhu::HeavyAttackDef FCultHeavyAttackData::ToCoreDef() const
{
	cultulhu::HeavyAttackDef D;
	switch (Kind)
	{
	case ECultHeavyAttackKind::MeleeHeavy:    D.kind = cultulhu::HeavyAttackKind::MeleeHeavy; break;
	case ECultHeavyAttackKind::MindControl:   D.kind = cultulhu::HeavyAttackKind::MindControl; break;
	case ECultHeavyAttackKind::AcidSpit:      D.kind = cultulhu::HeavyAttackKind::AcidSpit; break;
	case ECultHeavyAttackKind::EldritchGrasp: D.kind = cultulhu::HeavyAttackKind::EldritchGrasp; break;
	}
	D.name = ToStd(Name);
	D.damageMult = DamageMult;
	D.range = Range;
	D.ccType = ToStd(CcType);
	D.ccSeconds = CcSeconds;
	return D;
}

cultulhu::CharacterDef UCultCharacterData::ToCoreDef() const
{
	cultulhu::CharacterDef D;
	D.id = ToStd(CharacterId);
	D.displayName = ToStd(DisplayName);
	D.flavor = ToStd(Flavor);
	D.maxHp = MaxHp;
	D.moveSpeed = MoveSpeed;
	D.maxStamina = MaxStamina;
	D.qAbility = QAbility.ToCoreDef();
	D.fAbility = FAbility.ToCoreDef();
	D.rAbility = RAbility.ToCoreDef();
	D.meleeComboId = ToStd(MeleeComboId);
	D.rightClick = RightClick.ToCoreDef();
	D.rmbAbilityId = ToStd(RmbAbilityId);
	D.passiveId = ToStd(PassiveId);
	D.passiveDesc = ToStd(PassiveDesc);
	// VERIFY IN EDITOR: confirm CharacterDef field names (id, displayName,
	// flavor, maxHp, moveSpeed, maxStamina, qAbility/fAbility/rAbility,
	// meleeComboId, rightClick, rmbAbilityId, passiveId, passiveDesc)
	// still match the core.
	return D;
}
