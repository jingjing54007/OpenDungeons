/*
 *  Copyright (C) 2011-2016  OpenDungeons Team
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RENDERSCENESYNCWAIT_H
#define RENDERSCENESYNCWAIT_H

#include "renderscene/RenderScene.h"

class RenderSceneSyncWait : public RenderScene
{
public:
    RenderSceneSyncWait() :
        mIsWaiting(true)
    {}

    virtual ~RenderSceneSyncWait()
    {}

    const std::string& getModifierName() const override;

    bool activate(CameraManager& cameraManager, RenderManager& renderManager) override;
    bool update(CameraManager& cameraManager, RenderManager& renderManager,
        Ogre::Real timeSinceLastFrame) override;
    void freeRessources(CameraManager& cameraManager, RenderManager& renderManager) override;

    void notifySyncPost(const std::string& event);
    bool importFromStream(std::istream& is) override;

private:
    std::string mEvent;
    bool mIsWaiting;
};

#endif // RENDERSCENESYNCWAIT_H
