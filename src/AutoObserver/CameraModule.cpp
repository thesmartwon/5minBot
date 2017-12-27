#include "sc2api/sc2_api.h"
#include "CameraModule.h"
#include <iostream>

int TILE_SIZE = 32;

CameraModule::CameraModule(sc2::Agent & bot):
	m_bot(bot), 
	cameraMoveTime(150),
	cameraMoveTimeMin(50),
	watchScoutWorkerUntil(7500),
	lastMoved(0),
	lastMovedPriority(0),
	lastMovedPosition(sc2::Point2D(0, 0)),
	cameraFocusPosition(sc2::Point2D(0, 0)),
	cameraFocusUnit(nullptr),
	followUnit(false)
{
}

void CameraModule::onStart()
{
	setPlayerIds();
	setPlayerStartLocations();
	cameraFocusPosition = (*m_startLocations.begin()).second;
	currentCameraPosition = (*m_startLocations.begin()).second;
}

void CameraModule::onFrame()
{
	moveCameraFallingNuke();
	//moveCameraNukeDetect();
	moveCameraIsUnderAttack();
	moveCameraIsAttacking();
	if (m_bot.Observation()->GetGameLoop() <= watchScoutWorkerUntil)
	{
		moveCameraScoutWorker();
	}
	moveCameraDrop();
	moveCameraArmy();

	updateCameraPosition();
}

void CameraModule::moveCameraFallingNuke()
{
	int prio = 5;
	if (!shouldMoveCamera(prio))
	{
		return;
	}

	for(auto & effects: m_bot.Observation()->GetEffects())
	{
		if (effects.effect_id==uint32_t(7)) //7 = NukePersistent NOT TESTED YET
		{
			moveCamera(effects.positions.front(), prio);
			return;
		}
	}
}

//Not yet implemented
void CameraModule::moveCameraNukeDetect(const sc2::Point2D target)
{
	int prio = 4;
	if (!shouldMoveCamera(prio))
	{
		return;
	}
	else
	{
		moveCamera(target, prio);
	}
}

void CameraModule::moveCameraIsUnderAttack()
{
	const int prio = 3;
	if (!shouldMoveCamera(prio))
	{
		return;
	}

	for (auto unit : m_bot.Observation()->GetUnits())
	{
		if (isUnderAttack(unit))
		{
			moveCamera(unit, prio);
		}
	}
}


void CameraModule::moveCameraIsAttacking()
{
	int prio = 3;
	if (!shouldMoveCamera(prio))
	{
		return;
	}

	for (auto unit : m_bot.Observation()->GetUnits())
	{
		if (isAttacking(unit))
		{
			moveCamera(unit, prio);
		}
	}
}

void CameraModule::moveCameraScoutWorker()
{
	int highPrio = 2;
	int lowPrio = 0;
	if (!shouldMoveCamera(lowPrio))
	{
		return;
	}

	for (auto & unit : m_bot.Observation()->GetUnits())
	{
		if (!IsWorkerType(unit->unit_type))
		{
			continue;
		}
		if (isNearOpponentStartLocation(unit->pos,unit->owner))
		{
			moveCamera(unit, highPrio);
		}
		else if (!isNearOwnStartLocation(unit->pos,unit->owner))
		{
			moveCamera(unit, lowPrio);
		}
	}
}

void CameraModule::moveCameraDrop() {
	int prio = 2;
	if (!shouldMoveCamera(prio))
	{
		return;
	}
	for (auto & unit : m_bot.Observation()->GetUnits())
	{
		if ((unit->unit_type.ToType() == sc2::UNIT_TYPEID::ZERG_OVERLORDTRANSPORT || unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_MEDIVAC || unit->unit_type.ToType() == sc2::UNIT_TYPEID::PROTOSS_WARPPRISM)
			&& isNearOpponentStartLocation(unit->pos,unit->owner) && unit->cargo_space_taken > 0)
		{
			moveCamera(unit, prio);
		}
	}
}

void CameraModule::moveCameraArmy()
{
	int prio = 1;
	if (!shouldMoveCamera(prio))
	{
		return;
	}
	// Double loop, check if army units are close to each other
	int radius = 50;

	sc2::Point2D bestPos;
	const sc2::Unit * bestPosUnit = nullptr;
	int mostUnitsNearby = 0;

	for (auto & unit: m_bot.Observation()->GetUnits())
	{
		if (!isArmyUnitType(unit->unit_type.ToType()) || unit->display_type!=sc2::Unit::DisplayType::Visible || unit->alliance==sc2::Unit::Alliance::Neutral)
		{
			continue;
		}
		sc2::Point2D uPos = unit->pos;

		int nrUnitsNearby = 0;
		for (auto & nearbyUnit : m_bot.Observation()->GetUnits())
		{
			if (!isArmyUnitType(nearbyUnit->unit_type.ToType()) || unit->display_type != sc2::Unit::DisplayType::Visible || unit->alliance == sc2::Unit::Alliance::Neutral)
			{
				continue;
			}
			nrUnitsNearby++;
		}
		if (nrUnitsNearby > mostUnitsNearby) {
			mostUnitsNearby = nrUnitsNearby;
			bestPos = uPos;
			bestPosUnit = unit;
		}
	}

	if (mostUnitsNearby > 1) {
		moveCamera(bestPosUnit, prio);
	}
}

void CameraModule::moveCameraUnitCreated(const sc2::Unit * unit)
{
	int prio = 1;
	if (!shouldMoveCamera(prio) || unit->unit_type.ToType() == sc2::UNIT_TYPEID::TERRAN_KD8CHARGE)
	{
		return;
	}
	else if (!IsWorkerType(unit->unit_type))
	{
		moveCamera(unit, prio);
	}
}

const bool CameraModule::shouldMoveCamera(int priority) const
{
	const int elapsedFrames = m_bot.Observation()->GetGameLoop() - lastMoved;
	const bool isTimeToMove = elapsedFrames >= cameraMoveTime;
	const bool isTimeToMoveIfHigherPrio = elapsedFrames >= cameraMoveTimeMin;
	const bool isHigherPrio = lastMovedPriority < priority;
	// camera should move IF: enough time has passed OR (minimum time has passed AND new prio is higher)
	return isTimeToMove || (isHigherPrio && isTimeToMoveIfHigherPrio);
}

void CameraModule::moveCamera(sc2::Point2D pos, int priority)
{
	if (!shouldMoveCamera(priority))
	{
		return;
	}
	if (followUnit == false && cameraFocusPosition == pos)
	{
		// don't register a camera move if the position is the same
		return;
	}

	cameraFocusPosition = pos;
	lastMovedPosition = cameraFocusPosition;
	lastMoved = m_bot.Observation()->GetGameLoop();
	lastMovedPriority = priority;
	followUnit = false;
}

void CameraModule::moveCamera(const sc2::Unit * unit, int priority)
{
	if (!shouldMoveCamera(priority))
	{
		return;
	}
	if (followUnit == true && cameraFocusUnit == unit) {
		// don't register a camera move if we follow the same unit
		return;
	}

	cameraFocusUnit = unit;
	lastMovedPosition = cameraFocusUnit->pos;
	lastMoved = m_bot.Observation()->GetGameLoop();
	lastMovedPriority = priority;
	followUnit = true;
}




void CameraModule::updateCameraPosition()
{
	float moveFactor = 0.1f;
	if (followUnit && isValidPos(cameraFocusUnit->pos))
	{
		cameraFocusPosition = cameraFocusUnit->pos;
	}
	currentCameraPosition = currentCameraPosition + sc2::Point2D(
		moveFactor*(cameraFocusPosition.x - currentCameraPosition.x),
		moveFactor*(cameraFocusPosition.y - currentCameraPosition.y));
	sc2::Point2D currentMovedPosition = currentCameraPosition;

	if (isValidPos(currentCameraPosition))
	{
		m_bot.Debug()->DebugMoveCamera(currentMovedPosition);
	}
}

//Utility

//At the moment there is no flag for being under attack
const bool CameraModule::isUnderAttack(const sc2::Unit * unit) const
{
	return false;
}

const bool CameraModule::isAttacking(const sc2::Unit * unit) const
{
	//Option A
	return unit->orders.size()>0 && unit->orders.front().ability_id.ToType() == sc2::ABILITY_ID::ATTACK_ATTACK;
	//Option B
	//return unit->weapon_cooldown > 0.0f;
}

const bool CameraModule::IsWorkerType(const sc2::UNIT_TYPEID type) const
{
	switch (type)
	{
	case sc2::UNIT_TYPEID::TERRAN_SCV: return true;
	case sc2::UNIT_TYPEID::TERRAN_MULE: return true;
	case sc2::UNIT_TYPEID::PROTOSS_PROBE: return true;
	case sc2::UNIT_TYPEID::ZERG_DRONE: return true;
	case sc2::UNIT_TYPEID::ZERG_DRONEBURROWED: return true;
	default: return false;
	}
}

const bool CameraModule::isNearOpponentStartLocation(sc2::Point2D pos, int player) const
{
	return isNearOwnStartLocation(pos, getOpponent(player));
}

const bool CameraModule::isNearOwnStartLocation(const sc2::Point2D pos, int player) const
{
	int distance = 100;
	return Dist(pos, m_startLocations.at(player)) <= distance;
}

const bool CameraModule::isArmyUnitType(sc2::UNIT_TYPEID type) const
{
	if (IsWorkerType(type)) { return false; }
	if (type == sc2::UNIT_TYPEID::ZERG_OVERLORD) { return false; } //Excluded here the overlord transport etc to count them as army unit
	if (isBuilding(type)) { return false; }
	if (type == sc2::UNIT_TYPEID::ZERG_EGG) { return false; }
	if (type == sc2::UNIT_TYPEID::ZERG_LARVA) { return false; }

	return true;
}

const bool CameraModule::isBuilding(sc2::UNIT_TYPEID type) const
{
	switch(type)
	{
		//Terran
	case sc2::UNIT_TYPEID::TERRAN_ARMORY:
	case sc2::UNIT_TYPEID::TERRAN_BARRACKS:
	case sc2::UNIT_TYPEID::TERRAN_BARRACKSFLYING:
	case sc2::UNIT_TYPEID::TERRAN_BARRACKSREACTOR:
	case sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB:
	case sc2::UNIT_TYPEID::TERRAN_BUNKER:
	case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER:
	case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTERFLYING:
	case sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY:
	case sc2::UNIT_TYPEID::TERRAN_FACTORY:
	case sc2::UNIT_TYPEID::TERRAN_FACTORYFLYING:
	case sc2::UNIT_TYPEID::TERRAN_FACTORYREACTOR:
	case sc2::UNIT_TYPEID::TERRAN_FACTORYTECHLAB:
	case sc2::UNIT_TYPEID::TERRAN_FUSIONCORE:
	case sc2::UNIT_TYPEID::TERRAN_GHOSTACADEMY:
	case sc2::UNIT_TYPEID::TERRAN_MISSILETURRET:
	case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND:
	case sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING:
	case sc2::UNIT_TYPEID::TERRAN_PLANETARYFORTRESS:
	case sc2::UNIT_TYPEID::TERRAN_REFINERY:
	case sc2::UNIT_TYPEID::TERRAN_SENSORTOWER:
	case sc2::UNIT_TYPEID::TERRAN_STARPORT:
	case sc2::UNIT_TYPEID::TERRAN_STARPORTFLYING:
	case sc2::UNIT_TYPEID::TERRAN_STARPORTREACTOR:
	case sc2::UNIT_TYPEID::TERRAN_STARPORTTECHLAB:
	case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT:
	case sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED:
	case sc2::UNIT_TYPEID::TERRAN_REACTOR:
	case sc2::UNIT_TYPEID::TERRAN_TECHLAB:

		// Zerg
	case sc2::UNIT_TYPEID::ZERG_BANELINGNEST:
	case sc2::UNIT_TYPEID::ZERG_CREEPTUMOR:
	case sc2::UNIT_TYPEID::ZERG_CREEPTUMORBURROWED:
	case sc2::UNIT_TYPEID::ZERG_CREEPTUMORQUEEN:
	case sc2::UNIT_TYPEID::ZERG_EVOLUTIONCHAMBER:
	case sc2::UNIT_TYPEID::ZERG_EXTRACTOR:
	case sc2::UNIT_TYPEID::ZERG_GREATERSPIRE:
	case sc2::UNIT_TYPEID::ZERG_HATCHERY:
	case sc2::UNIT_TYPEID::ZERG_HIVE:
	case sc2::UNIT_TYPEID::ZERG_HYDRALISKDEN:
	case sc2::UNIT_TYPEID::ZERG_INFESTATIONPIT:
	case sc2::UNIT_TYPEID::ZERG_LAIR:
	case sc2::UNIT_TYPEID::ZERG_LURKERDENMP:
	case sc2::UNIT_TYPEID::ZERG_NYDUSCANAL:
	case sc2::UNIT_TYPEID::ZERG_NYDUSNETWORK:
	case sc2::UNIT_TYPEID::ZERG_SPAWNINGPOOL:
	case sc2::UNIT_TYPEID::ZERG_SPINECRAWLER:
	case sc2::UNIT_TYPEID::ZERG_SPINECRAWLERUPROOTED:
	case sc2::UNIT_TYPEID::ZERG_SPIRE:
	case sc2::UNIT_TYPEID::ZERG_SPORECRAWLER:
	case sc2::UNIT_TYPEID::ZERG_SPORECRAWLERUPROOTED:
	case sc2::UNIT_TYPEID::ZERG_ULTRALISKCAVERN:

		// Protoss
	case sc2::UNIT_TYPEID::PROTOSS_ASSIMILATOR:
	case sc2::UNIT_TYPEID::PROTOSS_CYBERNETICSCORE:
	case sc2::UNIT_TYPEID::PROTOSS_DARKSHRINE:
	case sc2::UNIT_TYPEID::PROTOSS_FLEETBEACON:
	case sc2::UNIT_TYPEID::PROTOSS_FORGE:
	case sc2::UNIT_TYPEID::PROTOSS_GATEWAY:
	case sc2::UNIT_TYPEID::PROTOSS_NEXUS:
	case sc2::UNIT_TYPEID::PROTOSS_PHOTONCANNON:
	case sc2::UNIT_TYPEID::PROTOSS_PYLON:
	case sc2::UNIT_TYPEID::PROTOSS_PYLONOVERCHARGED:
	case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSBAY:
	case sc2::UNIT_TYPEID::PROTOSS_ROBOTICSFACILITY:
	case sc2::UNIT_TYPEID::PROTOSS_STARGATE:
	case sc2::UNIT_TYPEID::PROTOSS_TEMPLARARCHIVE:
	case sc2::UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL:
	case sc2::UNIT_TYPEID::PROTOSS_WARPGATE:
		return true;
	}
	return false;
}

const bool CameraModule::isValidPos(const sc2::Point2D pos) const
{
	//Maybe playable width/height?
	return pos.x >= 0 && pos.y >= 0 && pos.x < m_bot.Observation()->GetGameInfo().width && pos.y < m_bot.Observation()->GetGameInfo().height;
}

const float CameraModule::Dist(const sc2::Unit * A, const sc2::Unit * B) const
{
	return std::sqrt(std::pow(A->pos.x - B->pos.x, 2) + std::pow(A->pos.y - B->pos.y, 2));
}

const float CameraModule::Dist(const sc2::Point2D A, const sc2::Point2D B) const
{
	return std::sqrt(std::pow(A.x - B.x, 2) + std::pow(A.y - B.y, 2));
}


void CameraModule::setPlayerStartLocations()
{
	
	std::vector<sc2::Point2D> startLocations = m_bot.Observation()->GetGameInfo().start_locations;
	sc2::Units bases = m_bot.Observation()->GetUnits(sc2::IsUnits({ sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER,sc2::UNIT_TYPEID::ZERG_HATCHERY,sc2::UNIT_TYPEID::PROTOSS_NEXUS }));
	// If we are not an observer
	// Assumes 2 player map
	if (bases.size() == 1)
	{
		for (auto & startLocation : startLocations)
		{
			if (Dist(bases.front()->pos, startLocation) < 20.0f)
			{
				m_startLocations[bases.front()->owner] = startLocation;
			}
			else
			{
				m_startLocations[getOpponent(bases.front()->owner)] = startLocation;
			}
		}
	}
	else
	{
		for (auto & unit : bases)
		{
			for (auto & startLocation : startLocations)
			{
				if (Dist(unit->pos, startLocation) < 20.0f)
				{
					m_startLocations[unit->owner] = startLocation;
				}
			}
		}
	}
}

void CameraModule::setPlayerIds()
{
	for (auto & player : m_bot.Observation()->GetGameInfo().player_info)
	{
		if (player.player_type != sc2::PlayerType::Observer)
		{
			m_playerIDs.push_back(player.player_id);
		}
	}
}

const int CameraModule::getOpponent(int player) const
{
	for (auto & i : m_playerIDs)
	{
		if (i != player)
		{
			return i;
		}
	}
}