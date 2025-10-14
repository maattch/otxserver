////////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////

#pragma once

#include "baseevents.h"
#include "condition.h"
#include "item.h"
#include "tile.h"

class Condition;
class Creature;
class Position;
class Item;

class TargetCallback;
class ValueCallback;
class TileCallback;

class CombatArea;
using CombatAreaPtr = std::unique_ptr<CombatArea>;

enum MatrixOperation_t : uint8_t
{
	MATRIXOPERATION_ROTATE90,
	MATRIXOPERATION_ROTATE180,
	MATRIXOPERATION_ROTATE270,
};

struct CombatParams
{
	std::vector<const Condition*> conditionList;

	std::unique_ptr<TargetCallback> targetCallback;
	std::unique_ptr<ValueCallback> valueCallback;
	std::unique_ptr<TileCallback> tileCallback;

	ConditionType_t dispelType = CONDITION_NONE;

	int32_t elementDamage = 0;
	uint16_t itemId = 0;

	MagicEffect_t impactEffect = MAGIC_EFFECT_NONE;
	MagicEffect_t hitEffect = MAGIC_EFFECT_NONE;
	ShootEffect_t distanceEffect = SHOOT_EFFECT_NONE;
	CombatType_t combatType = COMBAT_NONE;
	CombatType_t elementType = COMBAT_NONE;
	Color_t hitColor = COLOR_NONE;

	bool blockedByArmor = false;
	bool blockedByShield = false;
	bool targetCasterOrTopMost = false;
	bool targetPlayersOrSummons = false;
	bool differentAreaDamage = false;
	bool useCharges = true;
	bool isAggressive = true;
};

struct Combat2Var
{
	int32_t minChange, maxChange, change;
	Combat2Var() { minChange = maxChange = change = 0; }
};

class ValueCallback final : public CallBack
{
public:
	ValueCallback(FormulaType_t type) : m_type(type) {}

	void getMinMaxValues(Player* player, CombatDamage& damage, bool useCharges) const;

private:
	FormulaType_t m_type;
};

class TileCallback final : public CallBack
{
public:
	void onTileCombat(Creature* creature, Tile* tile) const;
};

class TargetCallback final : public CallBack
{
public:
	void onTargetCombat(Creature* creature, Creature* target) const;
};

using CombatFuncPtr = bool (*)(Creature* caster, Creature* target, const CombatParams& params, void*);

class MatrixArea final
{
public:
	MatrixArea() = default;
	MatrixArea(uint32_t rows, uint32_t cols) :
		m_array(rows * cols),
		m_rows(rows),
		m_cols(cols) {}

	uint8_t operator()(uint32_t row, uint32_t col) const { return m_array[row * m_cols + col]; }
	uint8_t& operator()(uint32_t row, uint32_t col) { return m_array[row * m_cols + col]; }

	void setCenter(uint32_t y, uint32_t x) {
		m_centerX = x;
		m_centerY = y;
	}

	MatrixArea construct(MatrixOperation_t operation) const;

	std::pair<uint32_t, uint32_t> getCenter() const { return { m_centerX, m_centerY }; }

	uint32_t getRows() const { return m_rows; }
	uint32_t getCols() const { return m_cols; }

private:
	MatrixArea(uint32_t x, uint32_t y, uint32_t rows, uint32_t cols, std::vector<uint8_t>&& arr) :
		m_array(std::move(arr)),
		m_rows(rows),
		m_cols(cols),
		m_centerX(x),
		m_centerY(y) {}

	std::vector<uint8_t> m_array;
	uint32_t m_rows = 0;
	uint32_t m_cols = 0;
	uint32_t m_centerX = 0;
	uint32_t m_centerY = 0;
};

class CombatArea final
{
public:
	void setupArea(const std::vector<uint8_t>& vec, uint32_t rows);
	void setupArea(uint8_t length, uint8_t spread);
	void setupArea(uint8_t radius);
	void setupExtArea(const std::vector<uint8_t>& vec, uint32_t rows);

	const MatrixArea& getArea(const Position& centerPos, const Position& targetPos) const;
	const MatrixArea& getAreaByDirection(Direction dir) const;

private:
	std::vector<MatrixArea> m_areas;
	bool m_hasExtArea = false;
};

class Combat final
{
public:
	Combat() = default;
	~Combat();

	static bool isInPvpZone(const Creature* attacker, const Creature* target);
	static bool isProtected(Player* attacker, Player* target);

	static CombatType_t ConditionToDamageType(ConditionType_t type);
	static ConditionType_t DamageToConditionType(CombatType_t type);

	static ReturnValue canTargetCreature(Player* attacker, Creature* target);
	static ReturnValue canDoCombat(Creature* caster, Tile* tile, bool isAggressive);
	static ReturnValue canDoCombat(Creature* attacker, Creature* target, bool isAggressive);

	static void doTargetCombat(Creature* caster, Creature* target, CombatDamage& damage, const CombatParams& params);
	static void doTargetCombat(Creature* caster, Creature* target, const CombatParams& params);
	static void doAreaCombat(Creature* caster, const Position& pos, const CombatArea* area, CombatDamage& damage, const CombatParams& params);
	static void doAreaCombat(Creature* caster, const Position& pos, const CombatArea* area, const CombatParams& params);

	static void postCombatEffects(Creature* caster, const Position& pos, const CombatParams& params);
	static void addDistanceEffect(Creature* caster, const Position& fromPos, const Position& toPos, ShootEffect_t effect);

	bool doCombat(Creature* caster, Creature* target) const;
	void doCombat(Creature* caster, const Position& pos) const;

	bool setCallback(CallBackParam_t key);
	CallBack* getCallback(CallBackParam_t key);

	void setArea(CombatAreaPtr area) { m_area = std::move(area); }

	bool hasArea() const { return m_area != nullptr; }

	bool setParam(CombatParam_t param, uint32_t value);
	void setCondition(const Condition* condition) { m_params.conditionList.push_back(condition); }
	void setPlayerCombatValues(FormulaType_t type, double mina, double minb, double maxa,
		double maxb, double minl, double maxl, double minm, double maxm, int32_t minc,
		int32_t maxc);

	void postCombatEffects(Creature* caster, const Position& pos) const {
		Combat::postCombatEffects(caster, pos, m_params);
	}

	CombatDamage getCombatDamage(Creature* creature) const;

private:
	static void combatTileEffects(const SpectatorVec& list, Creature* caster, Tile* tile, const CombatParams& params);

	CombatParams m_params;

	CombatAreaPtr m_area;

	// formula variables
	double m_mina = 0.0;
	double m_minb = 0.0;
	double m_maxa = 0.0;
	double m_maxb = 0.0;
	double m_minl = 0.0;
	double m_maxl = 0.0;
	double m_minm = 0.0;
	double m_maxm = 0.0;

	int32_t m_minc = 0;
	int32_t m_maxc = 0;

	FormulaType_t m_formulaType = FORMULA_UNDEFINED;
};

class MagicField final : public Item
{
public:
	MagicField(uint16_t type);

	MagicField* getMagicField() override { return this; }
	const MagicField* getMagicField() const override { return this; }

	bool isBlocking(const Creature* creature) const override;

	bool isReplacable() const { return Item::items[m_id].replacable; }
	bool isUnstepable() const { return m_id == ITEM_MAGICWALL_SAFE || m_id == ITEM_WILDGROWTH_SAFE; }

	CombatType_t getCombatType() const { return Item::items[m_id].combatType; }

	void onStepInField(Creature* creature);

private:
	int64_t createTime;
};
