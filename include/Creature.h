#ifndef CREATURE_H
#define CREATURE_H

#include <Ogre.h>
#include <string>
#include <deque>
using namespace std;
class Creature;

#include "Tile.h"
#include "CreatureAction.h"
#include "Weapon.h"
#include "Field.h"

/*! \brief Position, status, and AI state for a single game creature.
 *
 *  The creature class is the place where an individual creature's state is
 *  stored and manipulated.  The creature class is also used to store creature
 *  class descriptions, since a class decription is really just a subset of the
 *  overall creature information.  This is not really an optimal design and
 *  will probably be refined later but it works fine for now and the code
 *  affected by this change is relatively limited.
 */
class Creature
{
	public:
		// Constructors and operators
		Creature();
		Creature(string nClassName, string nMeshName, Ogre::Vector3 nScale, int nHP, int nMana, double nSightRadius, double nDigRate, double nMoveSpeed);

		// Class properties
		string className;
		string meshName;
		Ogre::Vector3 scale;
		double sightRadius;		// The inner radius where the creature sees everything
		double digRate;			// Fullness removed per turn of digging
		double danceRate;		// How much the danced upon tile's color changes per turn of dancing
		unsigned int level;
		double exp;
		double hpPerLevel;
		double manaPerLevel;

		// Individual properties
		string name;			// The creature's unique name
		Weapon *weaponL, *weaponR;	// The weapons the creature is holding
		string meshID, nodeID;		// The unique names for the OGRE entities
		int color;			// The color of the player who controls this creature
		int hp, maxHP, mana, maxMana;			// Basic stats
		double moveSpeed;		// How fast the creature moves and animates
		Tile::TileClearType tilePassability;	//FIXME:  This is not set from file yet.
		sem_t meshCreationFinishedSemaphore;
		sem_t meshDestructionFinishedSemaphore;

		// Object methods
		void createMesh();
		void destroyMesh();
		void deleteYourself();
		void setPosition(double x, double y, double z);
		void setPosition(Ogre::Vector3 v);
		Ogre::Vector3 getPosition();

		// AI stuff
		virtual void doTurn();
		double getHitroll(double range);
		double getDefense();
		void doLevelUp();
		vector<Tile*> visibleTiles;
		vector<Creature*> visibleEnemies;
		vector<Creature*> reachableEnemies;
		vector<Creature*> enemiesInRange;
		vector<Creature*> visibleAllies;
		vector<Creature*> reachableAllies;
		void updateVisibleTiles();
		vector<Creature*> getVisibleEnemies();
		vector<Creature*> getReachableCreatures(const vector<Creature*> &creaturesToCheck, unsigned int *minRange, Creature **nearestCreature);
		vector<Creature*> getEnemiesInRange(const vector<Creature*> &enemiesToCheck);
		vector<Creature*> getVisibleAllies();
		vector<Tile*> getVisibleMarkedTiles();
		vector<Creature*> getVisibleForce(int color, bool invert);
		Tile* positionTile();
		void setAnimationState(string s);
		AnimationState* getAnimationState();

		void addDestination(int x, int y);
		bool setWalkPath(list<Tile*> path, unsigned int minDestinations, bool addFirstStop);
		void clearDestinations();
		void clearActionQueue();
		void stopWalking();
		Player* getControllingPlayer();
		void computeBattlefield();

		// Visual debugging routines
		void createVisualDebugEntities();
		void destroyVisualDebugEntities();
		bool getHasVisualDebuggingEntities();

		static string getFormat();
		friend ostream& operator<<(ostream& os, Creature *c);
		friend istream& operator>>(istream& is, Creature *c);

		// Public data members
		AnimationState *animationState;
		string destinationAnimationState;
		double shortDistance;
		deque<Ogre::Vector3> walkQueue;
		Ogre::Vector3 walkDirection;
		SceneNode *sceneNode;

	private:
		deque<CreatureAction> actionQueue;
		Ogre::Vector3 position;
		int destinationX, destinationY;
		bool hasVisualDebuggingEntities;
		Tile *previousPositionTile;
		list<Tile*> visualDebugEntityTiles;
		Field *battleField;
		bool meshesExist;
};

#endif

