#include "GameCommander.h"
#include "CCBot.h"
#include "Util.h"

GameCommander::GameCommander(CCBot & bot)
    : m_bot                 (bot)
    , m_productionManager   (bot)
    , m_scoutManager        (bot)
	, m_harassManager		(bot)
    , m_combatCommander     (bot)
    , m_initialScoutSet     (false)
{

}

void GameCommander::onStart()
{
    m_productionManager.onStart();
    m_scoutManager.onStart();
	m_harassManager.onStart();
	m_combatCommander.onStart();
}

void GameCommander::onFrame()
{
    m_timer.start();

    handleUnitAssignments();
    m_productionManager.onFrame();
    m_scoutManager.onFrame();
	m_harassManager.onFrame();
    m_combatCommander.onFrame(m_combatUnits);

    drawDebugInterface();
}

void GameCommander::drawDebugInterface()
{
    drawGameInformation(4, 1);
}

void GameCommander::drawGameInformation(int x, int y)
{
    std::stringstream ss;
    ss << "Players: " << "\n";
    ss << "Strategy: " << m_bot.Config().StrategyName << "\n";
    ss << "Map Name: " << "\n";
    ss << "Time: " << "\n";
}

// assigns units to various managers
void GameCommander::handleUnitAssignments()
{
	m_validUnits.clear();
	m_combatUnits.clear();
	m_harassUnits.clear();
	// filter our units for those which are valid and usable
	setValidUnits();

	// set each type of unit
	setScoutUnits();
	setHarassUnits();
	setCombatUnits();
}

bool GameCommander::isAssigned(const sc2::Unit * unit) const
{
	return     (std::find(m_combatUnits.begin(), m_combatUnits.end(), unit) != m_combatUnits.end())
		|| (std::find(m_scoutUnits.begin(), m_scoutUnits.end(), unit) != m_scoutUnits.end())
		|| (std::find(m_harassUnits.begin(), m_harassUnits.end(), unit) != m_harassUnits.end());
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits()
{
	// make sure the unit is completed and alive and usable
	for (auto & unit : m_bot.UnitInfo().getUnits(Players::Self))
	{
		if (m_bot.GetUnit(unit->tag) && unit->is_alive && unit->last_seen_game_loop == m_bot.Observation()->GetGameLoop())
		{
			m_validUnits.push_back(unit);
		}
	}
}

void GameCommander::setScoutUnits()
{
	// if we haven't set a scout unit, do it
	if (false && m_scoutUnits.empty() && !m_initialScoutSet)
	{
		// if it exists
		if (shouldSendInitialScout())
		{
			// grab the closest worker to the supply provider to send to scout
			const sc2::Unit * workerScout = m_bot.Workers().getClosestMineralWorkerTo(m_bot.GetStartLocation());

			// if we find a worker (which we should) add it to the scout units
			if (workerScout)
			{
				m_scoutManager.setWorkerScout(workerScout);
				assignUnit(workerScout, m_scoutUnits);
				m_initialScoutSet = true;
			}
			else
			{

			}
		}
	}

	if (m_scoutManager.getNumScouts() == -1)
	{
		m_productionManager.requestScout();
		m_scoutManager.scoutRequested();
	}
	else if (m_scoutManager.getNumScouts() == 0)
	{
		for (auto & unit : m_validUnits)
		{
			BOT_ASSERT(unit, "Have a null unit in our valid units\n");

			if (!isAssigned(unit) && unit->unit_type == sc2::UNIT_TYPEID::TERRAN_REAPER)
			{
				m_scoutManager.setScout(unit);
				m_scoutUnits.clear();
				assignUnit(unit, m_scoutUnits);
				return;
			}
		}
	}
}

bool GameCommander::shouldSendInitialScout()
{
	return true;

	switch (m_bot.GetPlayerRace(Players::Self))
	{
	case sc2::Race::Terran:  return m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT, true) > 0;
	case sc2::Race::Protoss: return m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::PROTOSS_PYLON, true) > 0;
	case sc2::Race::Zerg:    return m_bot.UnitInfo().getUnitTypeCount(Players::Self, sc2::UNIT_TYPEID::ZERG_SPAWNINGPOOL, true) > 0;
	default: return false;
	}
}

void GameCommander::setHarassUnits()
{
	const sc2::Unit * medivac = m_harassManager.getMedivac();
	if (medivac)
	{
		assignUnit(medivac, m_harassUnits);
	}
	sc2::Units marines = m_harassManager.getMarines();
	if (marines.size()>0)
	{
		for (auto & m : marines)
		{
			assignUnit(m, m_harassUnits);
		}
	}
	const sc2::Unit * liberator = m_harassManager.getLiberator();
	if (liberator)
	{
		assignUnit(liberator, m_harassUnits);
	}
	sc2::Units enemies = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Enemy);
	for (auto & unit : m_validUnits)
	{
		BOT_ASSERT(unit, "Have a null unit in our valid units\n");

		if (!isAssigned(unit))
		{
			if (unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC && m_harassManager.needMedivac())
			{
				if (m_harassManager.setMedivac(unit))
				{
					assignUnit(unit, m_harassUnits);
				}
			}
			else if (unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_MARINE && unit->health==unit->health_max && m_harassManager.needMarine())
			{
				//If the marine is currently close to an anti air enemy, the medivac does not know what to do
				bool tooClose = false;
				for (auto & e : enemies)
				{
					if (Util::Dist(e->pos, unit->pos) < Util::GetUnitTypeSight(unit->unit_type,m_bot))
					{
						tooClose = true;
					}
				}
				if (!tooClose && m_harassManager.setMarine(unit))
				{
					assignUnit(unit, m_harassUnits);
				}
			}
			else if (unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_LIBERATOR && m_harassManager.needLiberator())
			{
				if (m_harassManager.setLiberator(unit))
				{
					assignUnit(unit, m_harassUnits);
				}
			}
		}
	}
}


// sets combat units to be passed to CombatCommander
void GameCommander::setCombatUnits()
{
    for (auto & unit : m_validUnits)
    {
        BOT_ASSERT(unit, "Have a null unit in our valid units\n");
        if (!isAssigned(unit) && Util::IsCombatUnitType(unit->unit_type, m_bot))
        {
			if (unit->cargo_space_taken > 0)
			{
				int a = 1;
			}
            assignUnit(unit, m_combatUnits);
        }
    }
}

void GameCommander::onUnitCreate(const sc2::Unit * unit)
{
	if (Util::IsCombatUnitType(unit->unit_type,m_bot))
	{
		sc2::Point2D pos(m_bot.Bases().getRallyPoint());
		if (Util::Dist(unit->pos, pos) > 5)
		{
			if (unit->unit_type == sc2::UNIT_TYPEID::TERRAN_MEDIVAC)
			{
				Micro::SmartMove(unit, m_bot.Bases().getRallyPoint(), m_bot);
				return;
			}
			else
			{
				const sc2::Units Bunker = m_bot.Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_BUNKER }));
				for (auto & b : Bunker)
				{
					if (b->build_progress==1.0f && b->cargo_space_taken != b->cargo_space_max)
					{
						Micro::SmartRightClick(unit, b, m_bot);
						m_bot.Actions()->UnitCommand(b, sc2::ABILITY_ID::LOAD, unit);
						return;
					}
				}
				Micro::SmartAttackMove(unit, m_bot.Bases().getRallyPoint(), m_bot);
				return;
			}
		}
	}
	else if (Util::IsTownHallType(unit->unit_type))
	{
		m_bot.Bases().assignTownhallToBase(unit);
	}
}

void GameCommander::OnBuildingConstructionComplete(const sc2::Unit * unit)
{
	if (Util::IsTownHallType(unit->unit_type))
	{
		m_bot.Bases().assignTownhallToBase(unit);
	}
}

void GameCommander::onUnitDestroy(const sc2::Unit * unit)
{
    //_productionManager.onUnitDestroy(unit);
}


void GameCommander::assignUnit(const sc2::Unit * unit, std::vector<const sc2::Unit *> & units)
{
    if (std::find(m_scoutUnits.begin(), m_scoutUnits.end(), unit) != m_scoutUnits.end())
    {
        m_scoutUnits.erase(std::remove(m_scoutUnits.begin(), m_scoutUnits.end(), unit), m_scoutUnits.end());
    }
    else if (std::find(m_combatUnits.begin(), m_combatUnits.end(), unit) != m_combatUnits.end())
    {
        m_combatUnits.erase(std::remove(m_combatUnits.begin(), m_combatUnits.end(), unit), m_combatUnits.end());
    }
	else if (std::find(m_harassUnits.begin(), m_harassUnits.end(), unit) != m_harassUnits.end())
	{
		m_harassUnits.erase(std::remove(m_harassUnits.begin(), m_harassUnits.end(), unit), m_harassUnits.end());
	}

    units.push_back(unit);
}

const ProductionManager & GameCommander::Production() const
{
	return m_productionManager;
}