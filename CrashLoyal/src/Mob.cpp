#include "Mob.h"

#include <memory>
#include <limits>
#include <stdlib.h>
#include <stdio.h>
#include "Building.h"
#include "Waypoint.h"
#include "GameState.h"
#include "Point.h"

int Mob::previousUUID;

Mob::Mob() 
	: pos(-10000.f,-10000.f)
	, nextWaypoint(NULL)
	, targetPosition(new Point)
	, state(MobState::Moving)
	, uuid(Mob::previousUUID + 1)
	, attackingNorth(true)
	, health(-1)
	, targetLocked(false)
	, target(NULL)
	, lastAttackTime(0)
{
	Mob::previousUUID += 1;
}

void Mob::Init(const Point& pos, bool attackingNorth)
{
	health = GetMaxHealth();
	this->pos = pos;
	this->attackingNorth = attackingNorth;
	findClosestWaypoint();
}

std::shared_ptr<Point> Mob::getPosition() {
	return std::make_shared<Point>(this->pos);
}

bool Mob::findClosestWaypoint() {
	std::shared_ptr<Waypoint> closestWP = GameState::waypoints[0];
	float smallestDist = std::numeric_limits<float>::max();

	for (std::shared_ptr<Waypoint> wp : GameState::waypoints) {
		//std::shared_ptr<Waypoint> wp = GameState::waypoints[i];
		// Filter out any waypoints that are "behind" us (behind is relative to attack dir
		// Remember y=0 is in the top left
		if (attackingNorth && wp->pos.y > this->pos.y) {
			continue;
		}
		else if ((!attackingNorth) && wp->pos.y < this->pos.y) {
			continue;
		}

		float dist = this->pos.dist(wp->pos);
		if (dist < smallestDist) {
			smallestDist = dist;
			closestWP = wp;
		}
	}
	std::shared_ptr<Point> newTarget = std::shared_ptr<Point>(new Point);
	this->targetPosition->x = closestWP->pos.x;
	this->targetPosition->y = closestWP->pos.y;
	this->nextWaypoint = closestWP;
	
	return true;
}

void Mob::moveTowards(std::shared_ptr<Point> moveTarget, double elapsedTime) {
	Point movementVector;
	movementVector.x = moveTarget->x - this->pos.x;
	movementVector.y = moveTarget->y - this->pos.y;
	movementVector.normalize();
	movementVector *= (float)this->GetSpeed();
	movementVector *= (float)elapsedTime;
	pos += movementVector;
}


void Mob::findNewTarget() {
	// Find a new valid target to move towards and update this mob
	// to start pathing towards it

	if (!findAndSetAttackableMob()) { findClosestWaypoint(); }
}

// Have this mob start aiming towards the provided target
// TODO: impliment true pathfinding here
void Mob::updateMoveTarget(std::shared_ptr<Point> target) {
	this->targetPosition->x = target->x;
	this->targetPosition->y = target->y;
}

void Mob::updateMoveTarget(Point target) {
	this->targetPosition->x = target.x;
	this->targetPosition->y = target.y;
}


// Movement related
//////////////////////////////////
// Combat related

int Mob::attack(int dmg) {
	this->health -= dmg;
	return health;
}

bool Mob::findAndSetAttackableMob() {
	// Find an attackable target that's in the same quardrant as this Mob
	// If a target is found, this function returns true
	// If a target is found then this Mob is updated to start attacking it
	for (std::shared_ptr<Mob> otherMob : GameState::mobs) {
		if (otherMob->attackingNorth == this->attackingNorth) { continue; }

		bool imLeft    = this->pos.x     < (SCREEN_WIDTH / 2);
		bool otherLeft = otherMob->pos.x < (SCREEN_WIDTH / 2);

		bool imTop    = this->pos.y     < (SCREEN_HEIGHT / 2);
		bool otherTop = otherMob->pos.y < (SCREEN_HEIGHT / 2);
		if ((imLeft == otherLeft) && (imTop == otherTop)) {
			// If we're in the same quardrant as the otherMob
			// Mark it as the new target
			this->setAttackTarget(otherMob);
			return true;
		}
	}
	return false;
}

// TODO Move this somewhere better like a utility class
int randomNumber(int minValue, int maxValue) {
	// Returns a random number between [min, max). Min is inclusive, max is not.
	return (rand() % maxValue) + minValue;
}

void Mob::setAttackTarget(std::shared_ptr<Attackable> newTarget) {
	this->state = MobState::Attacking;
	target = newTarget;
}

bool Mob::targetInRange() {
	float range = this->GetSize(); // TODO: change this for ranged units
	float totalSize = range + target->GetSize();
	return this->pos.insideOf(*(target->getPosition()), totalSize);
}
// Combat related
////////////////////////////////////////////////////////////
// Collisions

// PROJECT 3: 
//  1) return a vector of mobs that we're colliding with
//  2) handle collision with towers & river 
std::vector<std::shared_ptr<Attackable>> Mob::checkCollision() {
	std::vector<std::shared_ptr<Attackable>> collidingWith;
	Point minCorner(pos.x - GetSize(), pos.y - GetSize());
	for (std::shared_ptr<Mob> otherMob : GameState::mobs) {
		// don't collide with yourself
		if (this->sameMob(otherMob)) { continue; }

		Point otherMin(otherMob->pos.x - (otherMob->GetSize()), otherMob->pos.y - (otherMob->GetSize()));
		// PROJECT 3: YOUR CODE CHECKING FOR A COLLISION GOES HERE
		if (minCorner.x < otherMin.x + otherMob->GetSize() * 2 && minCorner.x + GetSize() * 2 > otherMin.x
			&& minCorner.y < otherMin.y + otherMob->GetSize() * 2 && minCorner.y + GetSize() * 2 > otherMin.y) {
			collidingWith.push_back(otherMob);
		}
	}

	for (std::shared_ptr<Building> b : GameState::buildings) {
		Point otherMin(b->getPoint().x - (b->GetSize() / 2), b->getPoint().y - (b->GetSize() / 2));

		if (minCorner.x < otherMin.x + b->GetSize() && minCorner.x + GetSize() > otherMin.x
			&& minCorner.y < otherMin.y + b->GetSize() && minCorner.y + GetSize() > otherMin.y) {
			collidingWith.push_back(b);
		}
	}

	return collidingWith;
}

void Mob::processCollision(std::vector<std::shared_ptr<Attackable>> otherObjs, double elapsedTime) {
	// PROJECT 3: YOUR COLLISION HANDLING CODE GOES HERE
	Point direction(targetPosition->x - pos.x, targetPosition->y - pos.y);
	direction.normalize();
	Point newVelocity(0, 0);
	for (std::shared_ptr<Attackable> otherObj : otherObjs) {

		Point otherDirection(targetPosition->x - pos.x, targetPosition->y - pos.y);
		otherDirection.normalize();
		Point moveDir(pos.x - otherObj->getPosition()->x, pos.y - otherObj->getPosition()->y);
		// Check if they are roughly going the same direction
		if (direction.x * otherDirection.x + direction.y * otherDirection.y > 0.85f) {
			if (direction.x * moveDir.x + direction.y * moveDir.y > 0.85f) {
				continue;
			}
		}

		float mag = (moveDir.x * moveDir.x) + (moveDir.y * moveDir.y);
		moveDir.x = moveDir.x * ((GetSize() * 2 + otherObj->GetSize())) / mag;
		moveDir.y = moveDir.y * ((GetSize() * 2 + otherObj->GetSize())) / mag;
		float massDiff = (otherObj->GetMass() / GetMass());
		moveDir *= (massDiff > GetSpeed() * 2) ? GetSpeed() * 2 : massDiff;
		if (abs(moveDir.x) < 0.01f) {
			moveDir.x += 0.5f;
		}
		newVelocity += moveDir;
	}
	pos += newVelocity * elapsedTime;
}

// Helper function that takes
void Mob::riverCollisions() {
	if (pos.y - GetSize() < RIVER_BOT_Y && pos.y + GetSize() > RIVER_TOP_Y) {
		if (pos.x - GetSize() < LEFT_BRIDGE_CENTER_X - BRIDGE_WIDTH / 2) {
			if (pos.y < RIVER_BOT_Y && pos.y > RIVER_TOP_Y) {

				pos = Point(LEFT_BRIDGE_CENTER_X - BRIDGE_WIDTH / 2 + GetSize(), pos.y);
			}
			else {
				float riverHeight = RIVER_BOT_Y - RIVER_TOP_Y;
				pos = Point(pos.x, (pos.y < RIVER_BOT_Y - riverHeight / 2) ? RIVER_TOP_Y - GetSize() : RIVER_BOT_Y + GetSize());
			}
		}
		else if (pos.x + GetSize() > RIGHT_BRIDGE_CENTER_X + BRIDGE_WIDTH / 2) {
			if (pos.y < RIVER_BOT_Y && pos.y > RIVER_TOP_Y) {
				pos = Point(RIGHT_BRIDGE_CENTER_X + BRIDGE_WIDTH / 2 - GetSize(), pos.y);
			}
			else {
				float riverHeight = RIVER_BOT_Y - RIVER_TOP_Y;
				pos = Point(pos.x, (pos.y < RIVER_BOT_Y - riverHeight / 2) ? RIVER_TOP_Y - GetSize() : RIVER_BOT_Y + GetSize());
			}
		}
		else if (pos.x + GetSize() > LEFT_BRIDGE_CENTER_X + BRIDGE_WIDTH / 2 && pos.x - GetSize() < RIGHT_BRIDGE_CENTER_X - BRIDGE_WIDTH / 2) {
			if (pos.y < RIVER_BOT_Y && pos.y > RIVER_TOP_Y) {
				pos = Point((pos.x > GAME_GRID_WIDTH / 2) ? RIGHT_BRIDGE_CENTER_X - BRIDGE_WIDTH / 2 + GetSize() : LEFT_BRIDGE_CENTER_X + BRIDGE_WIDTH / 2 - GetSize(), pos.y);
			}
			else {
				float riverHeight = RIVER_BOT_Y - RIVER_TOP_Y;
				pos = Point(pos.x, (pos.y < RIVER_BOT_Y - riverHeight / 2) ? RIVER_TOP_Y - GetSize() : RIVER_BOT_Y + GetSize());
			}
		}
	}
}

// Collisions
///////////////////////////////////////////////
// Procedures

void Mob::attackProcedure(double elapsedTime) {
	if (this->target == nullptr || this->target->isDead()) {
		this->targetLocked = false;
		this->target = nullptr;
		this->state = MobState::Moving;
		return;
	}

	if (targetInRange()) {
		if (this->lastAttackTime >= this->GetAttackTime()) {
			// If our last attack was longer ago than our cooldown
			this->target->attack(this->GetDamage());
			this->lastAttackTime = 0; // lastAttackTime is incremented in the main update function
			return;
		}
	}
	else {
		// If the target is not in range
		moveTowards(target->getPosition(), elapsedTime);
	}
}

void Mob::moveProcedure(double elapsedTime) {
	if (targetPosition) {
		moveTowards(targetPosition, elapsedTime);

		// Check for collisions
		if (this->nextWaypoint->pos.insideOf(this->pos, (this->GetSize() + WAYPOINT_SIZE))) {
			std::shared_ptr<Waypoint> trueNextWP = this->attackingNorth ?
												   this->nextWaypoint->upNeighbor :
												   this->nextWaypoint->downNeighbor;
			setNewWaypoint(trueNextWP);
		}

		// PROJECT 3: You should not change this code very much, but this is where your 
		// collision code will be called from
		std::vector<std::shared_ptr<Attackable>> otherMobs = this->checkCollision();
		if (otherMobs.size() > 0) {
			this->processCollision(otherMobs, elapsedTime);
		}
		this->riverCollisions();

		// Fighting otherMob takes priority always
		findAndSetAttackableMob();

	} else {
		// if targetPosition is nullptr
		findNewTarget();
	}
}

void Mob::update(double elapsedTime) {

	switch (this->state) {
	case MobState::Attacking:
		this->attackProcedure(elapsedTime);
		break;
	case MobState::Moving:
	default:
		this->moveProcedure(elapsedTime);
		break;
	}

	this->lastAttackTime += (float)elapsedTime;
}
