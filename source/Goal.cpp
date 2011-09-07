#include "Goal.h"
#include "AllGoals.h"

Goal::Goal(const std::string& nName, const std::string& nArguments, const GameMap& gameMap) :
        gameMap(gameMap),
        name(nName),
        arguments(nArguments)
        
{
}

void Goal::doSuccessAction()
{
}

bool Goal::isVisible()
{
    return true;
}

bool Goal::isUnmet(Seat *s)
{
    return !isMet(s);
}

bool Goal::isFailed(Seat *s)
{
    return false;
}

void Goal::addSuccessSubGoal(Goal *g)
{
    successSubGoals.push_back(g);
}

unsigned int Goal::numSuccessSubGoals() const
{
    return successSubGoals.size();
}

Goal* Goal::getSuccessSubGoal(int index)
{
    return successSubGoals[index];
}

void Goal::addFailureSubGoal(Goal *g)
{
    failureSubGoals.push_back(g);
}

unsigned int Goal::numFailureSubGoals() const
{
    return failureSubGoals.size();
}

Goal* Goal::getFailureSubGoal(int index)
{
    return failureSubGoals[index];
}

std::string Goal::getFormat()
{
    return "goalName\targuments";
}

std::ostream& operator<<(std::ostream& os, Goal *g)
{
    unsigned int subGoals;

    os << g->name << "\t";
    os << (g->arguments.size() > 0 ? g->arguments : "NULL") << "\n";

    subGoals = g->numSuccessSubGoals();
    if (subGoals > 0)
    {
        os << "+ " << subGoals << "\n";
        for (unsigned int i = 0; i < subGoals; ++i)
            os << g->getSuccessSubGoal(i);
    }

    subGoals = g->numFailureSubGoals();
    if (subGoals > 0)
    {
        os << "- " << subGoals << "\n";
        for (unsigned int i = 0; i < subGoals; ++i)
            os << g->getFailureSubGoal(i);
    }

    return os;
}

Goal* Goal::instantiateFromStream(std::istream& is, const GameMap& gameMap)
{
    std::string tempName, tempArguments;
    Goal *tempGoal = NULL;

    // Store the name and arguments of the goal so we can instantiate a specific goal subclass below.
    is >> tempName;
    getline(is, tempArguments);

    // Since getline leaves any leading whitespace we need to cut that off the beginning of the arguments string.
    int count = 0;
    while (tempArguments[count] == '\n' || tempArguments[count] == '\t'
            || tempArguments[count] == ' ')
        ++count;

    if (count > 0)
        tempArguments = tempArguments.substr(count, tempArguments.length());

    // Since entering an empty string in the file would break the file read we represent it with NULL and then substitute it here.
    if (tempArguments.compare("NULL") == 0)
        tempArguments = "";

    // Parse the goal type name to find out what subclass of goal tempGoal should be instantiated as.
    if (tempName.compare("KillAllEnemies") == 0)
    {
        tempGoal = new GoalKillAllEnemies(tempName, tempArguments, gameMap);
    }

    else if (tempName.compare("ProtectCreature") == 0)
    {
        tempGoal = new GoalProtectCreature(tempName, tempArguments, gameMap);
    }

    else if (tempName.compare("ClaimNTiles") == 0)
    {
        tempGoal = new GoalClaimNTiles(tempName, tempArguments, gameMap);
    }

    else if (tempName.compare("MineNGold") == 0)
    {
        tempGoal = new GoalMineNGold(tempName, tempArguments, gameMap);
    }

    else if (tempName.compare("ProtectDungeonTemple") == 0)
    {
        tempGoal = new GoalProtectDungeonTemple(tempName, tempArguments, gameMap);
    }

    // Now that the goal has been properly instantiated we check to see if there are subgoals to read in.
    char c;
    c = is.peek();
    int numSubgoals;
    if (c == '+')
    {
        // There is a subgoal which should be added on success.
        is.ignore(1);
        is >> numSubgoals;
        for (int i = 0; i < numSubgoals; ++i)
            tempGoal->addSuccessSubGoal(instantiateFromStream(is, gameMap));
    }
    else if (c == '-')
    {
        // There is a subgoal which should be added on failure.
        is.ignore(1);
        is >> numSubgoals;
        for (int i = 0; i < numSubgoals; ++i)
            tempGoal->addFailureSubGoal(instantiateFromStream(is, gameMap));
    }

    return tempGoal;
}
