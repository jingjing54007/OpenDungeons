#include <math.h>
#include <algorithm>

#include <Ogre.h>

#include "Creature.h"
#include "Defines.h"
#include "Globals.h"
#include "Functions.h"
#include "CreatureAction.h"
#include "Network.h"
#include "Field.h"
#include "Weapon.h"

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
#define snprintf _snprintf
#endif

Creature::Creature()
	: AnimatedObject()
{
	hasVisualDebuggingEntities = false;

	scale = Ogre::Vector3(1,1,1);
	sightRadius = 10.0;
	digRate = 10.0;
	exp = 0.0;
	level = 1;
	danceRate = 0.35;
	destinationX = 0;
	destinationY = 0;

	sem_init(&hpLockSemaphore, 0, 1);
	sem_wait(&hpLockSemaphore);
	hp = 10;
	sem_post(&hpLockSemaphore);
	sem_init(&manaLockSemaphore, 0, 1);
	sem_wait(&manaLockSemaphore);
	mana = 10;
	sem_post(&manaLockSemaphore);
	maxHP = 10;
	maxMana = 10;
	hpPerLevel = 0.0;
	manaPerLevel = 0.0;
	gold = 0;
	sightRadius = 10;
	digRate = 10;
	moveSpeed = 1.0;
	tilePassability = Tile::walkableTile;
	homeTile = NULL;

	weaponL = NULL;
	weaponR = NULL;

	sceneNode = NULL;

	actionQueue.push_back(CreatureAction(CreatureAction::idle));
	battleField = new Field("autoname");

	meshesExist = false;
}

/*  This function causes a segfault in Creature::doTurn() when computeBattlefield() is called.
Creature::~Creature()
{
	if(battleField != NULL)
	{
		delete battleField;
		battleField = NULL;
	}
}
*/

/** \brief A function which returns a string describing the IO format of the << and >> operators.
 *
*/
string Creature::getFormat()
{
	//NOTE:  When this format changes changes to RoomPortal::spawnCreature() may be necessary.
	string tempString = "className\tname\tposX\tposY\tposZ\tcolor\tweaponL";
	tempString += Weapon::getFormat();
	tempString += "\tweaponR";
	tempString += Weapon::getFormat();
	tempString += "\tHP\tmana";

	return tempString;
}

/*! \brief A matched function to transport creatures between files and over the network.
 *
 */
ostream& operator<<(ostream& os, Creature *c)
{
	os << c->className << "\t" << c->name << "\t";

	sem_wait(&c->positionLockSemaphore);
	os << c->position.x << "\t" << c->position.y << "\t" << c->position.z << "\t";
	sem_post(&c->positionLockSemaphore);

	os << c->color << "\t";
	os << c->weaponL << "\t" << c->weaponR << "\t";

	sem_wait(&c->hpLockSemaphore);
	os << c->hp << "\t";
	sem_post(&c->hpLockSemaphore);

	sem_wait(&c->manaLockSemaphore);
	os << c->mana;
	sem_post(&c->manaLockSemaphore);

	return os;
}

/*! \brief A matched function to transport creatures between files and over the network.
 *
 */
istream& operator>>(istream& is, Creature *c)
{
	double xLocation = 0.0, yLocation = 0.0, zLocation = 0.0;
	string tempString;
	is >> c->className;

	is >> tempString;

	if(tempString.compare("autoname") == 0)
		tempString = c->getUniqueCreatureName();

	c->name = tempString;

	is >> xLocation >> yLocation >> zLocation;
	sem_wait(&c->positionLockSemaphore);
	c->position = Ogre::Vector3(xLocation, yLocation, zLocation);
	sem_post(&c->positionLockSemaphore);

	is >> c->color;

	c->weaponL = new Weapon;
	is >> c->weaponL;
	c->weaponL->parentCreature = c;
	c->weaponL->handString = "L";

	c->weaponR = new Weapon;
	is >> c->weaponR;
	c->weaponR->parentCreature = c;
	c->weaponR->handString = "R";

	// Copy the class based items
	CreatureClass *creatureClass = gameMap.getClassDescription(c->className);
	if(creatureClass != NULL)
	{
		*c = *creatureClass;
	}

	sem_wait(&c->hpLockSemaphore);
	is >> c->hp;
	sem_post(&c->hpLockSemaphore);

	sem_wait(&c->manaLockSemaphore);
	is >> c->mana;
	sem_post(&c->manaLockSemaphore);

	return is;
}

Creature Creature::operator=(CreatureClass c2)
{
	className = c2.className;
	meshName = c2.meshName;
	scale = c2.scale;
	sightRadius = c2.sightRadius;
	digRate = c2.digRate;
	danceRate = c2.danceRate;
	hpPerLevel = c2.hpPerLevel;
	manaPerLevel = c2.manaPerLevel;
	moveSpeed = c2.moveSpeed;
	maxHP = c2.maxHP;
	maxMana = c2.maxMana;
	bedMeshName = c2.bedMeshName;
	bedDim1 = c2.bedDim1;
	bedDim2 = c2.bedDim2;

	return *this;
}

/*! \brief Allocate storage for, load, and inform OGRE about a mesh for this creature.
 *
 *  This function is called after a creature has been loaded from hard disk,
 *  received from a network connection, or created during the game play by the
 *  game engine itself.
 */
void Creature::createMesh()
{
	if(meshesExist)
		return;

	meshesExist = true;

	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::createCreature;
	request->p = this;

	// Add the request to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}


/*! \brief Free the mesh and inform the OGRE system that the mesh has been destroyed.
 *
 *  This function is primarily a helper function for other methods.
 */
void Creature::destroyMesh()
{
	if(!meshesExist)
		return;

	meshesExist = false;
	weaponL->destroyMesh();
	weaponR->destroyMesh();

	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::destroyCreature;
	request->p = this;

	// Add the request to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}

/*! \brief Changes the creature's position to a new position.
 *
 *  This is an overloaded function which just calls Creature::setPosition(double x, double y, double z).
 */
void Creature::setPosition(Ogre::Vector3 v)
{
	setPosition(v.x, v.y, v.z);
}

/*! \brief Changes the creature's position to a new position.
 *
 *  Moves the creature to a new location in 3d space.  This function is
 *  responsible for informing OGRE anything it needs to know, as well as
 *  maintaining the list of creatures in the individual tiles.
 */
void Creature::setPosition(double x, double y, double z)
{
	// If we are on the gameMap we may need to update the tile we are in
	if(gameMap.getCreature(name) != NULL)
	{
		// We are on the map
		// Move the creature relative to its parent scene node.  We record the
		// tile the creature is in before and after the move to properly
		// maintain the results returned by the positionTile() function.
		Tile *oldPositionTile = positionTile();
		sem_wait(&positionLockSemaphore);
		position = Ogre::Vector3(x, y, z);
		sem_post(&positionLockSemaphore);
		Tile *newPositionTile = positionTile();

		if(oldPositionTile != newPositionTile)
		{
			if(oldPositionTile != NULL)
				oldPositionTile->removeCreature(this);

			if(positionTile() != NULL)
				positionTile()->addCreature(this);
		}
	}
	else
	{
		// We are not on the map
		sem_wait(&positionLockSemaphore);
		position = Ogre::Vector3(x, y, z);
		sem_post(&positionLockSemaphore);
	}

	// Create a RenderRequest to notify the render queue that the scene node for this creature needs to be moved.
	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::moveSceneNode;
	request->str = name + "_node";
	request->vec = position;

	// Add the request to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}

void Creature::setHP(double nHP)
{
	sem_wait(&hpLockSemaphore);
	hp = nHP;
	sem_post(&hpLockSemaphore);
}

double Creature::getHP(Tile *tile)
{
	sem_wait(&hpLockSemaphore);
	double tempDouble = hp;
	sem_post(&hpLockSemaphore);

	return tempDouble;
}

void Creature::setMana(double nMana)
{
	sem_wait(&manaLockSemaphore);
	mana = nMana;
	sem_post(&manaLockSemaphore);
}

double Creature::getMana()
{
	sem_wait(&manaLockSemaphore);
	double tempDouble = mana;
	sem_post(&manaLockSemaphore);

	return tempDouble;
}

double Creature::getMoveSpeed()
{
	return moveSpeed;
}

/*! \brief The main AI routine which decides what the creature will do and carries out that action.
 *
 * The doTurn routine is the heart of the Creature AI subsystem.  The other,
 * higher level, functions such as GameMap::doTurn() ultimately just call this
 * function to make the creatures act.
 *
 * The function begins in a pre-cognition phase which prepares the creature's
 * brain state for decision making.  This involves generating lists of known
 * about creatures, either through sight, hearing, keeper knowledge, etc, as
 * well as some other bookkeeping stuff.
 *
 * Next the function enters the cognition phase where the creature's current
 * state is examined and a decision is made about what to do.  The state of the
 * creature is in the form of a queue, which is really used more like a stack.
 * At the beginning of the game the 'idle' action is pushed onto each
 * creature's actionQueue, this action is never removed from the tail end of
 * the queue and acts as a "last resort" for when the creature completely runs
 * out of things to do.  Other actions such as 'walkToTile' or 'attackObject'
 * are then pushed onto the front of the queue and will determine the
 * creature's future behavior.  When actions are complete they are popped off
 * the front of the action queue, causing the creature to revert back into the
 * state it was in when the actions was placed onto the queue.  This allows
 * actions to be carried out recursively, i.e. if a creature is trying to dig a
 * tile and it is not nearby it can begin walking toward the tile as a new
 * action, and when it arrives at the tile it will revert to the 'digTile'
 * action.
 *
 * In the future there should also be a post-cognition phase to do any
 * additional checks after it tries to move, etc.
 */
void Creature::doTurn()
{
	std::vector<Tile*> markedTiles;
	std::list<Tile*>walkPath;
	std::list<Tile*>basePath;
	std::list<Tile*>::iterator tileListItr;
	std::vector< std::list<Tile*> > possiblePaths;
	std::vector< std::list<Tile*> > shortPaths;
	bool loopBack;
	int tempInt;
	unsigned int tempUnsigned;
	unsigned int rangeToNearestEnemyObject, rangeToNearestAlliedObject;
	Creature *tempCreature;
	AttackableObject *tempAttackableObject;
	AttackableObject *nearestEnemyObject, *nearestAlliedObject;
	CreatureAction tempAction;
	Ogre::Vector3 tempVector;
	Quaternion tempQuat;

	// Heal.
	sem_wait(&hpLockSemaphore);
	hp += 0.25;
	if(hp > maxHP)
		hp = maxHP;
	sem_post(&hpLockSemaphore);
	
	// Regenrate mana.
	sem_wait(&manaLockSemaphore);
	mana += 0.75;
	if(mana > maxMana)
		mana = maxMana;
	sem_post(&manaLockSemaphore);

	// Check to see if we have earned enough experience to level up.
	while(exp >= 5*level + 5*powl(level/3, 2))
		doLevelUp();

	// If we are not standing somewhere on the map, do nothing.
	if(positionTile() == NULL)
		return;

	// Look at the surrounding area
	updateVisibleTiles();
	visibleEnemyObjects = getVisibleEnemyObjects();
	reachableEnemyObjects = getReachableAttackableObjects(visibleEnemyObjects, &rangeToNearestEnemyObject, &nearestEnemyObject);
	enemyObjectsInRange = getEnemyObjectsInRange(visibleEnemyObjects);
	visibleAlliedObjects = getVisibleAlliedObjects();
	reachableAlliedObjects = getReachableAttackableObjects(visibleAlliedObjects, &rangeToNearestAlliedObject, &nearestAlliedObject);
	if(digRate > 0.1)
		markedTiles = getVisibleMarkedTiles();

	// If the creature can see enemies that are reachable.
	if(reachableEnemyObjects.size() > 0)
	{
		// Check to see if there is any combat actions (maneuvering/attacking) in our action queue.
		bool alreadyFighting = false;
		for(unsigned int i = 0; i < actionQueue.size(); i++)
		{
			if(actionQueue[i].type == CreatureAction::attackObject || actionQueue[i].type == CreatureAction::maneuver)
			{
				alreadyFighting = true;
				break;
			}
		}

		// If we are not already fighting with a creature or maneuvering then start doing so.
		if(!alreadyFighting)
		{
			if(randomDouble(0.0, 1.0) < (1.0/(rangeToNearestEnemyObject) - digRate/80.0))
			{
				tempAction.type = CreatureAction::maneuver;
				actionQueue.push_front(tempAction);
			}
		}
	}

	// Check to see if we have found a "home" tile where we can sleep yet.
	if(digRate <= 0.1 && randomDouble(0.0, 1.0) < 0.03 && homeTile == NULL && actionQueue.front().type != CreatureAction::findHome)
	{
		// Check to see if there are any quarters owned by our color that we can reach.
		std::vector<Room*> tempRooms = gameMap.getRoomsByTypeAndColor(Room::quarters, color);
		tempRooms = gameMap.getReachableRooms(tempRooms, positionTile(), tilePassability);
		if(tempRooms.size() > 0)
		{
			tempAction.type = CreatureAction::findHome;
			actionQueue.push_front(tempAction);
		}
	}

	// The loopback variable allows creatures to begin processing a new
	// action immediately after some other action happens.
	do
	{
		loopBack = false;

		// Carry out the current task
		double diceRoll;
		double tempDouble;
		Tile *neighborTile;
		std::vector<Tile*>neighbors, neighbors2, creatureNeighbors, claimableTiles;
		bool wasANeighbor = false;
		Player *tempPlayer;
		Tile *tempTile, *tempTile2, *myTile;
		Room *tempRoom;
		std::list<Tile*> tempPath, tempPath2;
		pair<LocationType, double> minimumFieldValue;
		std::vector<Room*> treasuriesOwned;
		std::vector<Room*> tempRooms;

		diceRoll = randomDouble(0.0, 1.0);
		if(actionQueue.size() > 0)
		{
			switch(actionQueue.front().type)
			{
				case CreatureAction::idle:
					//cout << "idle ";
					setAnimationState("Idle");
					//FIXME: make this into a while loop over a vector of <action, probability> pairs

					// Decide to check for clamiable tiles
					if(diceRoll < 0.2 && danceRate > 0.1)
					{
						loopBack = true;
						actionQueue.push_front(CreatureAction(CreatureAction::claimTile));
					}

					// Decide to check for diggable tiles
					else if(diceRoll < 0.4 && digRate > 0.1)
					{
						loopBack = true;
						actionQueue.push_front(CreatureAction(CreatureAction::digTile));
					}

					// Decide to deposit the gold we are carrying into a treasury.
					else if(diceRoll < 0.4+0.6*(gold/(double)maxGoldCarriedByWorkers) && digRate > 0.1)
					{
						loopBack = true;
						actionQueue.push_front(CreatureAction(CreatureAction::depositGold));
					}

					// Decide to "wander" a short distance
					else if(diceRoll < 0.6)
					{
						loopBack = true;
						actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));

						// If we are not a worker.
						int tempX, tempY;
						bool workerFound = false;
						if(digRate < 0.1)
						{
							// Checkt to see if we want to try to follow a worker around or if we want to try to explore.
							if(randomDouble(0.0, 1.0) < 0.3)
							{
								// Try to find a worker to follow around.
								for(unsigned int i = 0; i < reachableAlliedObjects.size(); i++)
								{
									// Check to see if we found a worker.
									if(reachableAlliedObjects[i]->getAttackableObjectType() == AttackableObject::creature && \
											((Creature*)reachableAlliedObjects[i])->digRate > 0.1)
									{
										//TODO:  This should be improved so it picks the closest tile rather than just the [0] tile.
										tempTile = reachableAlliedObjects[i]->getCoveredTiles()[0];
										tempX = tempTile->x + 3.0*gaussianRandomDouble();
										tempY = tempTile->y + 3.0*gaussianRandomDouble();
										workerFound = true;
									}

									if(!workerFound)
									{
										sem_wait(&positionLockSemaphore);
										tempX = position.x + 2.0*gaussianRandomDouble();
										tempY = position.y + 2.0*gaussianRandomDouble();
										sem_post(&positionLockSemaphore);
									}
								}
							}
							else
							{
								// Try to find an unclaimed tile to walk to we choose this by the longest path to an unclaimed tile we find in the visible tiles we examine.
								//TODO: Make a copy of the visibleTiles and randomly choose tiles without replacement from this set to make the algorithm more balanced in the direction we walk.
								unsigned int maxLoopCount = randomUint(5, 15), longestPathLength = 0;
								std::list<Tile*> longestPath, tempPath;
								myTile = positionTile();
								for(unsigned int i = 0; i < visibleTiles.size() && i < maxLoopCount; i++)
								{
									tempPath = gameMap.path(myTile, visibleTiles[i], tilePassability);
									if(visibleTiles[i]->getType() == Tile::dirt && visibleTiles[i]->getFullness() == 0 && tempPath.size() >= 2 && tempPath.size() > longestPath.size())
									{
										longestPath = tempPath;
										longestPathLength = longestPath.size();
									}
								}

								if(longestPathLength >= 2)
								{
									gameMap.cutCorners(longestPath, tilePassability);
									setAnimationState("Walk");
									setWalkPath(longestPath, 2, false);
									break;
								}
							}
						}

						Tile *tempPositionTile = positionTile();
						std::list<Tile*> result;
						if(tempPositionTile != NULL)
						{
							result = gameMap.path(tempPositionTile->x, tempPositionTile->y, tempX, tempY, tilePassability);
						}

						gameMap.cutCorners(result, tilePassability);
						setAnimationState("Walk");
						setWalkPath(result, 2, false);
					}
					else
					{
						// Remain idle
						//setAnimationState("Idle");
					}

					break;

				case CreatureAction::walkToTile:
					/*
					if(reachableEnemyObjects.size() > 0 && rangeToNearestEnemyObject < 5)
					{
						actionQueue.pop_front();
						tempAction.type = CreatureAction::maneuver;
						actionQueue.push_front(tempAction);
						clearDestinations();
						loopBack = true;
						break;
					}
					*/

					//TODO: Peek at the item that caused us to walk
					// If we are walking toward a tile we are trying to dig out, check to see if it is still marked for digging.
					if(actionQueue[1].type == CreatureAction::digTile)
					{
						tempPlayer = getControllingPlayer();

						// Check to see if the tile is still marked for digging
						sem_wait(&walkQueueLockSemaphore);
						unsigned int index = walkQueue.size();
						Tile *currentTile = NULL;
						if(index > 0)
							currentTile = gameMap.getTile((int)walkQueue[index-1].x, (int)walkQueue[index-1].y);

						sem_post(&walkQueueLockSemaphore);

						if(currentTile != NULL)
						{
							// If it is not marked
							if(tempPlayer != NULL && !currentTile->getMarkedForDigging(tempPlayer))
							{
								// Clear the walk queue
								clearDestinations();
							}
						}
					}

					//cout << "walkToTile ";
					sem_wait(&walkQueueLockSemaphore);
					if(walkQueue.size() == 0)
					{
						actionQueue.pop_front();
						loopBack = true;

						// This extra post is included here because if the break statement happens
						// the one at the end of the 'if' block will not happen.
						sem_post(&walkQueueLockSemaphore);
						break;
					}
					sem_post(&walkQueueLockSemaphore);  // If this is removed remove the one in the 'if' block as well.
					break;

				case CreatureAction::claimTile:
					myTile = positionTile();
					//NOTE:  This is a workaround for the problem with the positionTile() function,
					// it can be removed when that issue is resolved.
					if(myTile == NULL)
					{
						actionQueue.pop_front();
						goto claimTileBreakStatement;
					}

					// Randomly decide to stop claiming with a small probability
					if(randomDouble(0.0, 1.0) < 0.1 + 0.2*markedTiles.size())
					{
						loopBack = true;
						actionQueue.pop_front();

						// If there are any visible tiles marked for digging start working on that.
						if(markedTiles.size() > 0)
							actionQueue.push_front(CreatureAction(CreatureAction::digTile));

						break;
					}

					// See if the tile we are standing on can be claimed
					if(myTile->color != color || myTile->colorDouble < 1.0)
					{
						//cout << "\nTrying to claim the tile I am standing on.";
						// Check to see if one of the tile's neighbors is claimed for our color
						neighbors = gameMap.neighborTiles(myTile);
						for(unsigned int j = 0; j < neighbors.size(); j++)
						{
							// Check to see if the current neighbor is already claimed
							tempTile = neighbors[j];
							if(tempTile->color == color && tempTile->colorDouble >= 1.0)
							{
								//cout << "\t\tFound a neighbor that is claimed.";
								// If we found a neighbor that is claimed for our side than we
								// can start dancing on this tile
								myTile->claimForColor(color, danceRate);

								// Since we danced on a tile we are done for this turn
								goto claimTileBreakStatement;
							}
						}
					}

					//cout << "\nLooking at the neighbor tiles to see if I can claim a tile.";
					// The tile we are standing on is already claimed or is not currently
					// claimable, find candidates for claiming.
					// Start by checking the neighbor tiles of the one we are already in
					neighbors = gameMap.neighborTiles(myTile);
					while(neighbors.size() > 0)
					{
						// If the current neigbor is claimable, walk into it and skip to the end of this turn
						tempInt = randomUint(0, neighbors.size()-1);
						tempTile = neighbors[tempInt];
						//NOTE:  I don't think the "colorDouble" check should happen here.
						if(tempTile != NULL && tempTile->getTilePassability() == Tile::walkableTile && (tempTile->color != color || tempTile->colorDouble < 1.0))
						{
							// The neighbor tile is a potential candidate for claiming, to be an actual candidate
							// though it must have a neighbor of its own that is already claimed for our side.
							neighbors2 = gameMap.neighborTiles(tempTile);
							for(unsigned int i = 0; i < neighbors2.size(); i++)
							{
								tempTile2 = neighbors2[i];
								if(tempTile2->color == color && tempTile2->colorDouble >= 1.0)
								{
									clearDestinations();
									addDestination(tempTile->x, tempTile->y);
									setAnimationState("Walk");
									goto claimTileBreakStatement;
								}
							}
						}

						neighbors.erase(neighbors.begin()+tempInt);
					}

					//cout << "\nLooking at the visible tiles to see if I can claim a tile.";
					// If we still haven't found a tile to claim, check the rest of the visible tiles
					for(unsigned int i = 0; i < visibleTiles.size(); i++)
					{
						// if this tile is not fully claimed yet or the tile is of another player's color
						tempTile = visibleTiles[i];
						if(tempTile != NULL && tempTile->getTilePassability() == Tile::walkableTile && (tempTile->colorDouble < 1.0 || tempTile->color != color))
						{
							// Check to see if one of the tile's neighbors is claimed for our color
							neighbors = gameMap.neighborTiles(visibleTiles[i]);
							for(unsigned int j = 0; j < neighbors.size(); j++)
							{
								tempTile = neighbors[j];
								if(tempTile->color == color && tempTile->colorDouble >= 1.0)
								{
									claimableTiles.push_back(tempTile);
								}
							}
						}
					}

					//cout << "  I see " << claimableTiles.size() << " tiles I can claim.";
					// Randomly pick a claimable tile, plot a path to it and walk to it
					while(claimableTiles.size() > 0)
					{
						// Randomly find a "good" tile to claim.  A good tile is one that has many neighbors
						// already claimed, this makes the claimed are more "round" and less jagged.
						tempUnsigned = 0;
						do
						{
							int numNeighborsClaimed;

							// Start by randomly picking a candidate tile.
							tempTile = claimableTiles[randomUint(0, claimableTiles.size()-1)];

							// Count how many of the candidate tile's neighbors are already claimed.
							neighbors = gameMap.neighborTiles(tempTile);
							numNeighborsClaimed = 0;
							for(unsigned int i = 0; i < neighbors.size(); i++)
							{
								if(neighbors[i]->color == color && neighbors[i]->colorDouble >= 1.0)
									numNeighborsClaimed++;
							}

							// Pick a random number in [0:1], if this number is high enough, than use this tile to claim.  The
							// bar for success approaches 0 as numTiles approaches N so this will be guaranteed to succeed at,
							// or before the time we get to the last unclaimed tile.  The bar for success is also lowered
							// according to how many neighbors are already claimed.
							//NOTE: The bar can be negative, when this happens we are guarenteed to use this candidate tile.
							double bar;
							bar = 1.0 - (numNeighborsClaimed/4.0) - (tempUnsigned/(double)(claimableTiles.size()-1));
							if(randomDouble(0.0, 1.0) >= bar)
								break;

							// Safety catch to prevent infinite loop in case the bar for success is too high and is never met.
							if(tempUnsigned >= claimableTiles.size()-1)
								break;

							// Increment the counter indicating how many candidate tiles we have rejected so far.
							tempUnsigned++;
						} while(true);

						if(tempTile != NULL)
						{
							// If we find a valid path to the tile start walking to it and break
							setAnimationState("Walk");
							tempPath = gameMap.path(myTile, tempTile, tilePassability);
							gameMap.cutCorners(tempPath, tilePassability);
							if(setWalkPath(tempPath, 2, false))
							{
								//loopBack = true;
								actionQueue.push_back(CreatureAction::walkToTile);
								goto claimTileBreakStatement;
							}
						}

						// If we got to this point, the tile we randomly picked cannot be gotten to via a
						// valid path.  Delete it from the claimable tiles vector and repeat the outer
						// loop to try to find another valid tile.
						for(unsigned int i = 0; i < claimableTiles.size(); i++)
						{
						        if(claimableTiles[i] == tempTile)
						        {
							    claimableTiles.erase(claimableTiles.begin() + i);
							    break;  // Break out of this for loop.
						        }
						}
					}

					// We couldn't find a tile to try to claim so we stop trying
					actionQueue.pop_front();
claimTileBreakStatement:
					break;

				case CreatureAction::digTile:
					tempPlayer = getControllingPlayer();
					//cout << "dig ";

					// Randomly decide to stop digging with a small probability
					if(randomDouble(0.0, 1.0) < 0.5 - 0.2*markedTiles.size())
					{
						loopBack = true;
						actionQueue.pop_front();
						goto claimTileBreakStatement;
					}

					// See if any of the tiles is one of our neighbors
					wasANeighbor = false;
					sem_wait(&positionLockSemaphore);
					creatureNeighbors = gameMap.neighborTiles(position.x, position.y);
					sem_post(&positionLockSemaphore);
					for(unsigned int i = 0; i < creatureNeighbors.size() && !wasANeighbor; i++)
					{
						if(tempPlayer != NULL && creatureNeighbors[i]->getMarkedForDigging(tempPlayer))
						{
							// If the tile is a gold tile accumulate gold for this creature.
							if(creatureNeighbors[i]->getType() == Tile::gold)
								gold += 25*min(digRate, (double)creatureNeighbors[i]->getFullness());

							// Turn so that we are facing toward the tile we are going to dig out.
							faceToward(creatureNeighbors[i]->x, creatureNeighbors[i]->y);

							// Dig out the tile by decreasing the tile's fullness.
							setAnimationState("Dig");
							creatureNeighbors[i]->setFullness(max(0.0, creatureNeighbors[i]->getFullness()-digRate));

							// Force all the neighbors to recheck their meshes as we may have exposed
							// a new side that was not visible before.
							neighbors = gameMap.neighborTiles(creatureNeighbors[i]);
							for(unsigned int j = 0; j < neighbors.size(); j++)
							{
								neighbors[j]->setFullness(neighbors[j]->getFullness());
							}

							// If the tile has been dug out, move into that tile and idle
							if(creatureNeighbors[i]->getFullness() == 0)
							{
								recieveExp(2);
								addDestination(creatureNeighbors[i]->x, creatureNeighbors[i]->y);
								creatureNeighbors[i]->setType(Tile::dirt);
								setAnimationState("Walk");

								// Remove the dig action and replace it with
								// walking to the newly dug out tile.
								actionQueue.pop_front();
								actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));
							}

							wasANeighbor = true;
							break;
						}
					}

					// Check to see if we are carrying the maximum amount of gold we can carry, and if so, try to take it to a treasury.
					if(gold >= maxGoldCarriedByWorkers)
					{
						// Remove the dig action and replace it with a depositGold action.
						actionQueue.pop_front();
						actionQueue.push_front(CreatureAction(CreatureAction::depositGold));
					}

					// If we successfully dug a tile then we are done for this turn.
					if(wasANeighbor)
						break;

					// Randomly decide to stop digging with a larger probability
					if(randomDouble(0.0, 1.0) < 0.3)
					{
						loopBack = true;
						actionQueue.pop_front();
						goto claimTileBreakStatement;
					}

					// Find paths to all of the neighbor tiles for all of the marked visible tiles.
					possiblePaths.clear();
					for(unsigned int i = 0; i < markedTiles.size(); i++)
					{
						neighbors = gameMap.neighborTiles(markedTiles[i]);
						for(unsigned int j = 0; j < neighbors.size(); j++)
						{
							neighborTile = neighbors[j];
							if(neighborTile != NULL && neighborTile->getFullness() == 0)
								possiblePaths.push_back(gameMap.path(positionTile(), neighborTile, tilePassability));

						}
					}

					// Find the shortest path and start walking toward the tile to be dug out
					if(possiblePaths.size() > 0)
					{
						// Find the N shortest valid paths, see if there are any valid paths shorter than this first guess
						shortPaths.clear();
						for(unsigned int i = 0; i < possiblePaths.size(); i++)
						{
							// If the current path is long enough to be valid
							unsigned int currentLength = possiblePaths[i].size();
							if(currentLength >= 2)
							{
								shortPaths.push_back(possiblePaths[i]);

								// If we already have enough short paths
								if(shortPaths.size() > 5)
								{
									unsigned int longestLength, longestIndex;

									// Kick out the longest
									longestLength = shortPaths[0].size();
									longestIndex = 0;
									for(unsigned int j = 1; j < shortPaths.size(); j++)
									{
										if(shortPaths[j].size() > longestLength)
										{
											longestLength = shortPaths.size();
											longestIndex = j;
										}
									}

									shortPaths.erase(shortPaths.begin() + longestIndex);
								}
							}
						}

						// Randomly pick a short path to take
						unsigned int numShortPaths = shortPaths.size();
						if(numShortPaths > 0)
						{
							unsigned int shortestIndex;
							shortestIndex = randomUint(0, numShortPaths-1);
							walkPath = shortPaths[shortestIndex];

							// If the path is a legitimate path, walk down it to the tile to be dug out
							gameMap.cutCorners(walkPath, tilePassability);
							if(setWalkPath(walkPath, 2, false))
							{
								setAnimationState("Walk");
								actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));
								break;
							}
						}
					}

					// If none of our neighbors are marked for digging we got here too late.
					// Finish digging
					if(actionQueue.front().type == CreatureAction::digTile)
					{
						actionQueue.pop_front();
						loopBack = true;
					}
					break;

				case CreatureAction::depositGold:
					// Check to see if we are standing in a treasury.
					myTile = positionTile();
					if(myTile != NULL)
					{
						tempRoom = myTile->getCoveringRoom();
						if(tempRoom != NULL && tempRoom->getType() == Room::treasury)
						{
							// Deposit as much of the gold we are carrying as we can into this treasury.
							gold -= ((RoomTreasury*)tempRoom)->depositGold(gold, myTile);

							// Depending on how much gold we have left (what did not fit in this treasury) we may want to continue
							// looking for another treasury to put the gold into.  Roll a dice to see if we want to quit looking not.
							if(randomDouble(1.0, maxGoldCarriedByWorkers) > gold)
							{
								actionQueue.pop_front();
								break;
							}
						}
					}
					else
					{
						break;
					}

					// We were not standing in a treasury that has enough room for the gold we are carrying, so try to find one to walk to.
					// Check to see if our seat controls any treasuries.
					treasuriesOwned = gameMap.getRoomsByTypeAndColor(Room::treasury, color);
					if(treasuriesOwned.size() > 0)
					{
						Tile *nearestTreasuryTile;  nearestTreasuryTile = NULL;
						unsigned int nearestTreasuryDistance;
						bool validPathFound;  validPathFound = false;
						tempPath.clear();
						tempPath2.clear();
						// Loop over the treasuries to find the closest one.
						for(unsigned int i = 0; i < treasuriesOwned.size(); i++)
						{
							if(!validPathFound)
							{
								// We have not yet found a valid path to a treasury, check to see if we can get to this treasury.
								tempUnsigned = randomUint(0, treasuriesOwned[i]->numCoveredTiles()-1);
								nearestTreasuryTile = treasuriesOwned[i]->getCoveredTile(tempUnsigned);
								tempPath = gameMap.path(myTile, nearestTreasuryTile, tilePassability);
								if(tempPath.size() >= 2 && ((RoomTreasury*)treasuriesOwned[i])->emptyStorageSpace() > 0)
								{
									validPathFound = true;
									nearestTreasuryDistance = tempPath.size();
								}
							}
							else
							{
								// We have already found at least one valid path to a treasury, see if this one is closer.
								tempUnsigned = randomUint(0, treasuriesOwned[i]->numCoveredTiles()-1);
								tempTile = treasuriesOwned[i]->getCoveredTile(tempUnsigned);
								tempPath2 = gameMap.path(myTile, tempTile, tilePassability);
								if(tempPath2.size() >= 2 && tempPath2.size() < nearestTreasuryDistance \
										&& ((RoomTreasury*)treasuriesOwned[i])->emptyStorageSpace() > 0)
								{
									tempPath = tempPath2;
									nearestTreasuryDistance = tempPath.size();
								}
							}
						}
						
						if(validPathFound)
						{
							// Begin walking to this treasury.
							gameMap.cutCorners(tempPath, tilePassability);
							if(setWalkPath(tempPath, 2, false))
							{
								setAnimationState("Walk");
								actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));
								loopBack = true;
								break;
							}
						}
					}
					else
					{
						// There are no treasuries available so just go back to what we were doing.
						actionQueue.pop_front();
						loopBack = true;
						break;
					}

					// If we get to here, there is either no treasuries controlled by us, or they are all
					// unreachable, or they are all full, so quit trying to deposit gold.
					actionQueue.pop_front();
					loopBack = true;
					break;

				case CreatureAction::findHome:
					// Check to see if we are standing in an open quarters tile that we can claim as our home.
					myTile = positionTile();
					if(myTile != NULL)
					{
						tempRoom = myTile->getCoveringRoom();
						if(tempRoom != NULL && tempRoom->getType() == Room::quarters)
						{
							if(((RoomQuarters*)tempRoom)->claimTileForSleeping(myTile, this))
								homeTile = myTile;
						}
					}
					else
					{
						break;
					}

					// If we found a tile to claim as our home in the above block.
					if(homeTile != NULL)
					{
						actionQueue.pop_front();
						loopBack = true;
						break;
					}

					// Check to see if we can walk to a quarters that does have an open tile.
					tempRooms = gameMap.getRoomsByTypeAndColor(Room::quarters, color);
					std::random_shuffle(tempRooms.begin(), tempRooms.end());
					unsigned int nearestQuartersDistance;
					bool validPathFound;  validPathFound = false;
					tempPath.clear();
					tempPath2.clear();
					for(unsigned int i = 0; i < tempRooms.size(); i++)
					{
						// Get the list of open rooms at the current quarters and check to see if
						// there is a place where we could put a bed big enough to sleep in.
						tempTile = ((RoomQuarters*)tempRooms[i])->getLocationForBed(bedDim1, bedDim2);

						// If the previous attempt to place the bed in this quarters failed, try again with the bed the other way.
						if(tempTile == NULL)
							tempTile = ((RoomQuarters*)tempRooms[i])->getLocationForBed(bedDim2, bedDim1);

						// Check to see if either of the two possible bed orientations tried above resulted in a successful placement.
						if(tempTile != NULL)
						{
							tempPath2 = gameMap.path(myTile, tempTile, tilePassability);

							// Find out the minimum valid path length of the paths determined in the above block.
							if(!validPathFound)
							{
								// If the current path is long enough to be valid then record the path and the distance.
								if(tempPath2.size() >= 2)
								{
									tempPath = tempPath2;
									nearestQuartersDistance = tempPath.size();
									validPathFound = true;
								}
							}
							else
							{
								// If the current path is long enough to be valid but shorter than the
								// shortest path seen so far, then record the path and the distance.
								if(tempPath2.size() >= 2 && tempPath2.size() < nearestQuartersDistance)
								{
									tempPath = tempPath2;
									nearestQuartersDistance = tempPath.size();
								}
							}
						}
					}

					// If we found a valid path to an open room in a quarters, then start walking along it.
					if(validPathFound)
					{
						gameMap.cutCorners(tempPath, tilePassability);
						if(setWalkPath(tempPath, 2, false))
						{
							setAnimationState("Walk");
							actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));
							loopBack = true;
							break;
						}
					}

					// If we got here there are no reachable quarters that are unclaimed so we quit trying to find one.
					actionQueue.pop_front();
					loopBack = true;
					break;

				case CreatureAction::attackObject:
					// If there are no more enemies which are reachable, stop attacking
					if(reachableEnemyObjects.size() == 0)
					{
						actionQueue.pop_front();
						loopBack = true;
						break;
					}

					myTile = positionTile();

					// Find the first enemy close enough to hit and attack it
					if(enemyObjectsInRange.size() > 0)
					{
						tempAttackableObject = enemyObjectsInRange[0];

						// Turn to face the creature we are attacking and set the animation state to Attack.
						//TODO:  This should be improved so it picks the closest tile rather than just the [0] tile.
						tempTile = tempAttackableObject->getCoveredTiles()[0];
						faceToward(tempTile->x, tempTile->y);
						setAnimationState("Attack1");

						// Calculate how much damage we do.
						//TODO:  This ignores the range of the creatures, fix this.
						double damageDone = getHitroll(0); // gameMap.crowDistance(this, tempCreature));
						damageDone *= randomDouble(0.0, 1.0);
						damageDone -= powl(randomDouble(0.0, 0.4), 2.0)*tempAttackableObject->getDefense();

						// Make sure the damage is positive.
						if(damageDone < 0.0)  damageDone = 0.0;

						// Do the damage and award experience points to both creatures.
						tempAttackableObject->takeDamage(damageDone, tempTile);
						double expGained;
						expGained = 1.0 + 0.2*powl(damageDone, 1.3);

						// Give a small amount of experince to the creature we hit.
						tempAttackableObject->recieveExp(0.15*expGained);

						// Add a bonus modifier based on the level of the creature we hit
						// to expGained and give ourselves that much experience.
						if(tempAttackableObject->getLevel() >= level)
							expGained *= 1.0 + (tempAttackableObject->getLevel() - level)/10.0;
						else
							expGained /= 1.0 + (level - tempAttackableObject->getLevel())/10.0;

						recieveExp(expGained);

						cout << "\n" << name << " did " << damageDone << " damage to " << enemyObjectsInRange[0]->getName();
						cout << " who now has " << enemyObjectsInRange[0]->getHP(tempTile) << "hp";

						// Randomly decide to start maneuvering again so we don't just stand still and fight.
						if(randomDouble(0.0, 1.0) <= 0.6)
							actionQueue.pop_front();

						break;
					}

					// There is not an enemy within range, begin maneuvering to try to get near an enemy, or out of the combat situation.
					actionQueue.push_front(CreatureAction(CreatureAction::maneuver));
					loopBack = true;
					break;

				case CreatureAction::maneuver:
					myTile = positionTile();

					// If there are no more enemies which are reachable, stop maneuvering.
					if(reachableEnemyObjects.size() == 0)
					{
						actionQueue.pop_front();
						loopBack = true;
						break;
					}

					// Check to see if we should try to strafe the enemy
					if(randomDouble(0.0, 1.0) < 0.3)
					{
						//TODO:  This should be improved so it picks the closest tile rather than just the [0] tile.
						tempTile = nearestEnemyObject->getCoveredTiles()[0];
						tempVector = Ogre::Vector3(tempTile->x, tempTile->y, 0.0);
						sem_wait(&positionLockSemaphore);
						tempVector -= position;
						sem_post(&positionLockSemaphore);
						tempVector.normalise();
						tempVector *= randomDouble(0.0, 3.0);
						tempQuat.FromAngleAxis(Ogre::Degree((randomDouble(0.0, 1.0) < 0.5 ? 90 : 270)), Ogre::Vector3::UNIT_Z);
						tempTile = gameMap.getTile(positionTile()->x + tempVector.x, positionTile()->y + tempVector.y);
						if(tempTile != NULL)
						{
							tempPath = gameMap.path(positionTile(), tempTile, tilePassability);

							if(setWalkPath(tempPath, 2, false))
								setAnimationState("Walk");
						}
					}

					// If there is an enemy within range, stop maneuvering and attack it.
					if(enemyObjectsInRange.size() > 0)
					{
						actionQueue.pop_front();
						loopBack = true;

						// If the next action down the stack is not an attackObject action, add it.
						if(actionQueue.front().type != CreatureAction::attackObject)
							actionQueue.push_front(CreatureAction(CreatureAction::attackObject));

						break;
					}

					// There are no enemy creatures in range so we will have to maneuver towards one.
					// Prepare the battlefield so we can decide where to move.
					computeBattlefield();

					cout << "\n\nMy name is " << name << "\tbattlefield score is " << battleField->get(myTile->x, myTile->y).first;

					// Find location on the battlefield, we try to find a minumum if we are
					// trying to "attack" and a maximum if we are trying to "retreat".
					if(battleField->get(myTile->x, myTile->y).first > 0.0)
					{
						minimumFieldValue = battleField->min();  // Attack
						setAnimationState("Walk");
						//TODO: Set this to some sort of Attack-move animation.
					}
					else
					{
						minimumFieldValue = battleField->max();  // Retreat
						setAnimationState("Flee");
					}

					// Pick a destination tile near the tile we got from the battlefield.
					clearDestinations();
					tempDouble = max(weaponL->range, weaponR->range);  // Pick a true destination randomly within the max range of our weapons.
					tempDouble = sqrt(tempDouble);
					//FIXME:  This should find a path to a tile we can walk to, it does not always do this the way it is right now.
					tempPath = gameMap.path(positionTile()->x, positionTile()->y, minimumFieldValue.first.first + randomDouble(-1.0*tempDouble,tempDouble), minimumFieldValue.first.second + randomDouble(-1.0*tempDouble, tempDouble), tilePassability);

					// Walk a maximum of N tiles before recomputing the destination since we are in combat.
					tempUnsigned = max((double)5, rangeToNearestEnemyObject/0.4);
					if(tempPath.size() >= tempUnsigned)
						tempPath.resize(tempUnsigned);

					gameMap.cutCorners(tempPath, tilePassability);
					if(setWalkPath(tempPath, 2, false))
						setAnimationState("Walk");

					// Push a walkToTile action into the creature's action queue to make them walk the path they have
					// decided on without recomputing, this helps prevent them from getting stuck in local minima.
					actionQueue.push_front(CreatureAction(CreatureAction::walkToTile));

					// This is a debugging statement, it produces a visual display of the battlefield as seen by the first creature.
					if(battleField->name.compare("field_1") == 0)
					{
						//battleField->refreshMeshes(1.0);
					}
					break;

				default:
					cerr << "\n\nERROR:  Unhandled action type in Creature::doTurn().\n\n";
					exit(1);
					break;
			}
		}
		else
		{
			cerr << "\n\nERROR:  Creature has empty action queue in doTurn(), this should not happen.\n\n";
			exit(1);
		}

	} while(loopBack);

	// Update the visual debugging entities
	if(hasVisualDebuggingEntities)
	{
		// if we are standing in a different tile than we were last turn
		Tile *currentPositionTile = positionTile();
		if(currentPositionTile != previousPositionTile)
		{
			//TODO:  This destroy and re-create is kind of a hack as its likely only a few tiles will actually change.
			destroyVisualDebugEntities();
			createVisualDebugEntities();
		}
	}
}

double Creature::getHitroll(double range)
{
	double tempHitroll = 1.0;

	if(weaponL != NULL && weaponL->range >= range)  tempHitroll += weaponL->damage;
	if(weaponR != NULL && weaponR->range >= range)  tempHitroll += weaponR->damage;
	tempHitroll *= log((double)log((double)level+1)+1);

	return tempHitroll;
}

double Creature::getDefense()
{
	double returnValue = 3.0;
	if(weaponL != NULL)  returnValue += weaponL->defense;
	if(weaponR != NULL)  returnValue += weaponR->defense;

	return returnValue;
}

void Creature::doLevelUp()
{
	level++;
	cout << "\n\n" << name << " has reached level " << level << "\n\n";

	if(digRate > 0.1)
		digRate *= 1.0 + log((double)log((double)level+1)+1);

	if(digRate > 60)  digRate = 60;

	maxHP += hpPerLevel;
	maxMana += manaPerLevel;

	// Scale up the mesh.
	if(meshesExist && level < 100)
	{
		double scaleFactor = 1.0 + level/200.0;
		if(scaleFactor > 1.05)  scaleFactor = 1.05;
		RenderRequest *request = new RenderRequest;
		request->type = RenderRequest::scaleSceneNode;
		request->p = sceneNode;
		request->vec = Ogre::Vector3(scaleFactor, scaleFactor, scaleFactor);

		// Add the request to the queue of rendering operations to be performed before the next frame.
		queueRenderRequest(request);
	}
}

/*! \brief Creates a list of Tile pointers in visibleTiles
 *
 * The tiles are currently determined to be visible or not, according only to
 * the distance they are away from the creature.  Because of this they can
 * currently see through walls, etc.
*/
void Creature::updateVisibleTiles()
{
	visibleTiles = gameMap.visibleTiles(positionTile(), sightRadius);
}

/*! \brief Loops over the visibleTiles and adds all enemy creatures in each tile to a list which it returns.
 *
*/
std::vector<AttackableObject*> Creature::getVisibleEnemyObjects()
{
	return getVisibleForce(color, true);
}

/*! \brief Loops over objectsToCheck and returns a vector containing all the ones which can be reached via a valid path.
 *
*/
std::vector<AttackableObject*> Creature::getReachableAttackableObjects(const std::vector<AttackableObject*> &objectsToCheck, unsigned int *minRange, AttackableObject **nearestObject)
{
	std::vector<AttackableObject*> tempVector;
	Tile *myTile = positionTile(), *objectTile;
	std::list<Tile*> tempPath;
	bool minRangeSet = false;

	// Loop over the vector of objects we are supposed to check.
	for(unsigned int i = 0; i < objectsToCheck.size(); i++)
	{
		// Try to find a valid path from the tile this creature is in to the nearest tile where the current target object is.
		//TODO:  This should be improved so it picks the closest tile rather than just the [0] tile.
		objectTile = objectsToCheck[i]->getCoveredTiles()[0];
		tempPath = gameMap.path(myTile, objectTile, tilePassability);

		// If the path we found is valid, then add the creature to the ones we return.
		if(tempPath.size() >= 2)
		{
			tempVector.push_back(objectsToCheck[i]);

			if(minRange != NULL)
			{
				if(!minRangeSet)
				{
					*nearestObject = objectsToCheck[i];
					*minRange = tempPath.size();
					minRangeSet = true;
				}
				else
				{
					if(tempPath.size() < *minRange)
					{
						*minRange = tempPath.size();
						*nearestObject = objectsToCheck[i];
					}
				}
			}
		}
	}

	//TODO: Maybe think of a better canary value for this.
	if(!minRangeSet)
		*minRange = 999999;

	return tempVector;
}

/*! \brief Loops over the enemyObjectsToCheck vector and adds all enemy creatures within weapons range to a list which it returns.
 *
*/
std::vector<AttackableObject*> Creature::getEnemyObjectsInRange(const std::vector<AttackableObject*> &enemyObjectsToCheck)
{
	std::vector<AttackableObject*> tempVector;

	// If there are no enemies to check we are done.
	if(enemyObjectsToCheck.size() == 0)
		return tempVector;

	// Find our location and calculate the square of the max weapon range we have.
	Tile *myTile = positionTile();
	double weaponRangeSquared = max(weaponL->range, weaponR->range);
	weaponRangeSquared *= weaponRangeSquared;

	// Loop over the enemyObjectsToCheck and add any within range to the tempVector.
	for(unsigned int i = 0; i < enemyObjectsToCheck.size(); i++)
	{
		//TODO:  This should be improved so it picks the closest tile rather than just the [0] tile.
		Tile *tempTile = enemyObjectsToCheck[i]->getCoveredTiles()[0];
		if(tempTile != NULL)
		{
			double rSquared = powl(myTile->x - tempTile->x, 2.0) + powl(myTile->y - tempTile->y, 2.0);

			if(rSquared < weaponRangeSquared)
				tempVector.push_back(enemyObjectsToCheck[i]);
		}
	}

	return tempVector;
}

/*! \brief Loops over the visibleTiles and adds all allied creatures in each tile to a list which it returns.
 *
*/
std::vector<AttackableObject*> Creature::getVisibleAlliedObjects()
{
	return getVisibleForce(color, false);
}

/*! \brief Loops over the visibleTiles and adds any which are marked for digging to a vector which it returns.
 *
*/
std::vector<Tile*> Creature::getVisibleMarkedTiles()
{
	std::vector<Tile*> tempVector;
	Player *tempPlayer = getControllingPlayer();

	// Loop over all the visible tiles.
	for(unsigned int i = 0; i < visibleTiles.size(); i++)
	{
		// Check to see if the tile is marked for digging.
		if(tempPlayer != NULL && visibleTiles[i]->getMarkedForDigging(tempPlayer))
			tempVector.push_back(visibleTiles[i]);
	}
	
	return tempVector;
}

/*! \brief Loops over the visibleTiles and returns any creatures in those tiles whose color matches (or if invert is true, does not match) the given color parameter.
 *
*/
std::vector<AttackableObject*> Creature::getVisibleForce(int color, bool invert)
{
	return gameMap.getVisibleForce(visibleTiles, color, invert);
}

/*! \brief Displays a mesh on all of the tiles visible to the creature.
 *
*/
void Creature::createVisualDebugEntities()
{
	hasVisualDebuggingEntities = true;
	visualDebugEntityTiles.clear();

	Tile *currentTile = NULL;
	updateVisibleTiles();
	for(unsigned int i = 0; i < visibleTiles.size(); i++)
	{
		currentTile = visibleTiles[i];

		if(currentTile != NULL)
		{
			// Create a render request to create a mesh for the current visible tile.
			RenderRequest *request = new RenderRequest;
			request->type = RenderRequest::createCreatureVisualDebug;
			request->p = currentTile;
			request->p2 = this;

			// Add the request to the queue of rendering operations to be performed before the next frame.
			queueRenderRequest(request);

			visualDebugEntityTiles.push_back(currentTile);

		}
	}
}

/*! \brief Destroy the meshes created by createVisualDebuggingEntities().
 *
*/
void Creature::destroyVisualDebugEntities()
{
	hasVisualDebuggingEntities = false;

	Tile *currentTile = NULL;
	updateVisibleTiles();
	std::list<Tile*>::iterator itr;
	for(itr = visualDebugEntityTiles.begin(); itr != visualDebugEntityTiles.end(); itr++)
	{
		currentTile = *itr;

		if(currentTile != NULL)
		{
			// Destroy the mesh for the current visible tile
			RenderRequest *request = new RenderRequest;
			request->type = RenderRequest::destroyCreatureVisualDebug;
			request->p = currentTile;
			request->p2 = this;

			// Add the request to the queue of rendering operations to be performed before the next frame.
			queueRenderRequest(request);
		}
	}

}

/*! \brief Returns a pointer to the tile the creature is currently standing in.
 *
*/
Tile* Creature::positionTile()
{
	sem_wait(&positionLockSemaphore);
	Ogre::Vector3 tempPosition(position);
	sem_post(&positionLockSemaphore);

	return gameMap.getTile((int)(tempPosition.x), (int)(tempPosition.y));
}

/** \brief Returns a vector containing the tile the creature is in, this is to conform to the AttackableObject interface.
 *
*/
std::vector<Tile*> Creature::getCoveredTiles()
{
	std::vector<Tile*> tempVector;
	tempVector.push_back(positionTile());
	return tempVector;
}

/*! \brief Completely destroy this creature, including its OGRE entities, scene nodes, etc.
 *
*/
void Creature::deleteYourself()
{
	// Make sure the weapons are deleted as well.
	weaponL->deleteYourself();
	weaponR->deleteYourself();

	// If we are standing in a valid tile, we need to notify that tile we are no longer there.
	if(positionTile() != NULL)
		positionTile()->removeCreature(this);

	if(meshesExist)
		destroyMesh();

	// Create a render request asking the render queue to actually do the deletion of this creature.
	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::deleteCreature;
	request->p = this;

	// Add the requests to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}

string Creature::getUniqueCreatureName()
{
	static int uniqueNumber = 1;
	string tempString = className + Ogre::StringConverter::toString(uniqueNumber);
	uniqueNumber++;
	return tempString;
}

/*! \brief Sets a new animation state from the creature's library of animations.
 *
*/
void Creature::setAnimationState(string s)
{
	string tempString;
	std::stringstream tempSS;
	RenderRequest *request = new RenderRequest;
	request->type = RenderRequest::setCreatureAnimationState;
	request->p = this;
	request->str = s;

	if(serverSocket != NULL)
	{
		try
		{
			// Place a message in the queue to inform the clients about the new animation state
			ServerNotification *serverNotification = new ServerNotification;
			serverNotification->type = ServerNotification::creatureSetAnimationState;
			serverNotification->str = s;
			serverNotification->cre = this;

			queueServerNotification(serverNotification);
		}
		catch(bad_alloc&)
		{
			cerr << "\n\nERROR:  bad alloc in Creature::setAnimationState\n\n";
			exit(1);
		}
	}

	// Add the request to the queue of rendering operations to be performed before the next frame.
	queueRenderRequest(request);
}

/*! \brief Returns the creature's currently active animation state.
 *
*/
AnimationState* Creature::getAnimationState()
{
	return animationState;
}

/** \brief Returns whether or not this creature is capable of moving, this is to conform to the AttackableObject interface.
 *
*/
bool Creature::isMobile()
{
	return true;
}

/** \brief Returns the creature's level, this is to conform to the AttackableObject interface.
 *
*/
int Creature::getLevel()
{
	return level;
}

/** \brief Returns the creature's color, this is to conform to the AttackableObject interface.
 *
*/
int Creature::getColor()
{
	return color;
}

/** \brief Sets the creature's color.
 *
*/
void Creature::setColor(int nColor)
{
	color = nColor;
}

/** \brief Returns the type of AttackableObject that this is (Creature, Room, etc), this is to conform to the AttackableObject interface.
 *
*/
AttackableObject::AttackableObjectType Creature::getAttackableObjectType()
{
	return AttackableObject::creature;
}

/** \brief Returns the name of this creature, this is to conform to the AttackableObject interface.
 *
*/
string Creature::getName()
{
	return name;
}

/** \brief Deducts a given amount of HP from this creature, this is to conform to the AttackableObject interface.
 *
*/
void Creature::takeDamage(double damage, Tile *tileTakingDamage)
{
	sem_wait(&hpLockSemaphore);
	hp -= damage;
	sem_post(&hpLockSemaphore);
}

/** \brief Adds experience to this creature, this is to conform to the AttackableObject interface.
 *
*/
void Creature::recieveExp(double experience)
{
	exp += experience;
}

/*! \brief An accessor to return whether or not the creature has OGRE entities for its visual debugging entities.
 *
*/
bool Creature::getHasVisualDebuggingEntities()
{
	return hasVisualDebuggingEntities;
}

/*! \brief Returns the first player whose color matches this creature's color.
 *
*/
//FIXME: This should be made into getControllingSeat(), when this is done it can simply be a call to GameMap::getSeatByColor().
Player* Creature::getControllingPlayer()
{
	Player *tempPlayer;

	if(gameMap.me->seat->color == color)
	{
		return gameMap.me;
	}

	// Try to find and return a player with color equal to this creature's
	for(unsigned int i = 0; i < gameMap.numPlayers(); i++)
	{
		tempPlayer = gameMap.getPlayer(i);
		if(tempPlayer->seat->color == color)
		{
			return tempPlayer;
		}
	}

	// No player found, return NULL
	return NULL;
}

/*! \brief Clears the action queue, except for the Idle action at the end.
 *
*/
void Creature::clearActionQueue()
{
	actionQueue.clear();
	actionQueue.push_back(CreatureAction(CreatureAction::idle));
}

/** \brief This function loops over the visible tiles and computes a score for each one indicating how
  * frindly or hostile that tile is and stores it in the battleField variable.
  *
*/
void Creature::computeBattlefield()
{
	Tile *myTile, *tempTile;
	int xDist, yDist;

	// Loop over the tiles in this creature's battleField and compute their value.
	// The creature will then walk towards the tile with the minimum value to
	// attack or towards the maximum value to retreat.
	myTile = positionTile();
	battleField->clear();
	for(unsigned int i = 0; i < visibleTiles.size(); i++)
	{
		tempTile = visibleTiles[i];
		double tileValue = 0.0;// - sqrt(rSquared)/sightRadius;

		// Enemies
		for(unsigned int j = 0; j < reachableEnemyObjects.size(); j++)
		{
			//TODO:  This should be improved so it picks the closest tile rather than just the [0] tile.
			Tile *tempTile2 = reachableEnemyObjects[j]->getCoveredTiles()[0];

			// Compensate for how close the creature is to me
			//rSquared = powl(myTile->x - tempTile2->x, 2.0) + powl(myTile->y - tempTile2->y, 2.0);
			//double factor = 1.0 / (sqrt(rSquared) + 1.0);

			// Subtract for the distance from the enemy creature to r
			xDist = tempTile->x - tempTile2->x;
			yDist = tempTile->y - tempTile2->y;
			tileValue -= 1.0 / sqrt((double)(xDist*xDist + yDist*yDist + 1));
		}

		// Allies
		for(unsigned int j = 0; j < visibleAlliedObjects.size(); j++)
		{
			//TODO:  This should be improved so it picks the closest tile rather than just the [0] tile.
			Tile *tempTile2 = visibleAlliedObjects[j]->getCoveredTiles()[0];

			// Compensate for how close the creature is to me
			//rSquared = powl(myTile->x - tempTile2->x, 2.0) + powl(myTile->y - tempTile2->y, 2.0);
			//double factor = 1.0 / (sqrt(rSquared) + 1.0);

			xDist = tempTile->x - tempTile2->x;
			yDist = tempTile->y - tempTile2->y;
			tileValue += 0.5 / (sqrt((double)(xDist*xDist + yDist*yDist + 1)));
		}

		const double jitter = 0.00;
		const double tileScaleFactor = 0.5;
		battleField->set(tempTile->x, tempTile->y, (tileValue + randomDouble(-1.0*jitter, jitter))*tileScaleFactor);
	}
}

