/*!
 * \file   MiniMap.cpp
 * \date   13 April 2011
 * \author StefanP.MUC
 * \brief  Contains everything that is related to the minimap
 *
 *
 *  Copyright (C) 2011-2014  OpenDungeons Team
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

#include "MiniMap.h"

#include "ODApplication.h"
#include "RenderManager.h"
#include "Gui.h"
#include "Creature.h"
#include "Helper.h"

#include <OgrePrerequisites.h>
#include <OgreTextureManager.h>

#include <CEGUI/WindowManager.h>
#include <CEGUI/Image.h>
#include <CEGUI/PropertyHelper.h>
#include <CEGUI/Texture.h>
#include <CEGUI/ImageManager.h>
#include <CEGUI/Size.h>
#include <CEGUI/BasicImage.h>

#include <cstdlib>

MiniMap::MiniMap(GameMap* gm) :
    mWidth(0),
    mHeight(0),
    mTopLeftCornerX(0),
    mTopLeftCornerY(0),
    mGrainSize(4),
    mTiles(NULL),
    mGameMap(gm),
    mPixelBox(NULL),
    mSheetUsed(Gui::guiSheet::mainMenu)
{
}

MiniMap::~MiniMap()
{
    if(mPixelBox != NULL)
        delete mPixelBox;

    if (mTiles != NULL)
        delete [] mTiles;
}

void MiniMap::attachMiniMap(Gui::guiSheet sheet)
{
    // If is configured with the same sheet, no need to rebuild
    if((mPixelBox != NULL) && (mSheetUsed == sheet))
        return;

    if(mPixelBox != NULL)
    {
        // The MiniMap has already been initialised. We free it
        Gui::getSingleton().sheets[mSheetUsed]->getChild(Gui::MINIMAP)->setProperty("Image", "");
        Ogre::TextureManager::getSingletonPtr()->remove("miniMapOgreTexture");
        CEGUI::ImageManager::getSingletonPtr()->destroy("MiniMapImageset");
        CEGUI::System::getSingletonPtr()->getRenderer()->destroyTexture("miniMapTextureGui");

        delete mPixelBox;
    }

    mSheetUsed = sheet;
    mWidth = Gui::getSingleton().sheets[sheet]->getChild(Gui::MINIMAP)->getPixelSize().d_width;
    mHeight = Gui::getSingleton().sheets[sheet]->getChild(Gui::MINIMAP)->getPixelSize().d_height;
    mTopLeftCornerX = Gui::getSingleton().sheets[sheet]->getChild(Gui::MINIMAP)->getUnclippedOuterRect().get().getPosition().d_x;
    mTopLeftCornerY = Gui::getSingleton().sheets[sheet]->getChild(Gui::MINIMAP)->getUnclippedOuterRect().get().getPosition().d_y;
    mPixelBox = new Ogre::PixelBox(mWidth, mHeight, 1, Ogre::PF_R8G8B8);

    allocateMiniMapMemory();

    // Image blank_image( Geometry(400, 300), Color(MaxRGB, MaxRGB, MaxRGB, 0));
    mMiniMapOgreTexture = Ogre::TextureManager::getSingletonPtr()->createManual(
            "miniMapOgreTexture",
            Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
            Ogre::TEX_TYPE_2D,
            mWidth, mHeight, 0, Ogre::PF_R8G8B8,
            Ogre::TU_DYNAMIC_WRITE_ONLY);

    mPixelBuffer = mMiniMapOgreTexture->getBuffer();

    CEGUI::Texture& miniMapTextureGui = static_cast<CEGUI::OgreRenderer*>(CEGUI::System::getSingletonPtr()
                                            ->getRenderer())->createTexture("miniMapTextureGui", mMiniMapOgreTexture);

    CEGUI::BasicImage& imageset = dynamic_cast<CEGUI::BasicImage&>(CEGUI::ImageManager::getSingletonPtr()->create("BasicImage", "MiniMapImageset"));
    imageset.setArea(CEGUI::Rectf(CEGUI::Vector2f(0.0, 0.0), CEGUI::Size<float>((float)mWidth, (float)mHeight)));

    // Link the image to the minimap
    imageset.setTexture(&miniMapTextureGui);

    CEGUI::String str = Gui::getSingleton().sheets[sheet]->getChild(Gui::MINIMAP)->getProperty("Image");
    Gui::getSingleton().sheets[sheet]->getChild(Gui::MINIMAP)->setProperty("Image", CEGUI::PropertyHelper<CEGUI::Image*>::toString(&imageset));

    mMiniMapOgreTexture->load();
}

void MiniMap::allocateMiniMapMemory()
{
    if (mTiles)
        delete [] mTiles;

    mTiles = new Color* [mHeight];
    for (Ogre::uint jj = 0; jj < mHeight; ++jj)
    {
        mTiles[jj] = new Color[mWidth];
    }
}

void MiniMap::updateCameraInfos(const Ogre::Vector3& vv, const double& rotation)
{
    mCamera_2dPosition = Ogre::Vector2(vv.x, vv.y);
    mCosRotation = cos(rotation - (PI/2.0));
    mSinRotation = sin(rotation - (PI/2.0));
}

Ogre::Vector2 MiniMap::camera_2dPositionFromClick(int xx, int yy)
{
    Ogre::Real mm, nn, oo, pp;
    // Compute move
    mm = ((yy - mTopLeftCornerY) / double(mHeight) - 0.5);
    nn = ((xx - mTopLeftCornerX) / double(mWidth) - 0.5);
    // Applying rotation
    oo = mm * mCosRotation - nn * mSinRotation;
    pp = mm * mSinRotation + nn * mCosRotation;
    // Apply result to camera
    mCamera_2dPosition.x += (Ogre::Real)(oo * mHeight / mGrainSize);
    mCamera_2dPosition.y += (Ogre::Real)(pp * mWidth / mGrainSize);

    return mCamera_2dPosition;
}

void MiniMap::swap()
{
    mPixelBuffer->lock(*mPixelBox, Ogre::HardwareBuffer::HBL_NORMAL);

    Ogre::uint8* pDest;
    pDest = static_cast<Ogre::uint8*>(mPixelBuffer->getCurrentLock().data) - 1;

    for (Ogre::uint ii = 0; ii < mWidth; ++ii)
    {
        for (Ogre::uint jj = 0; jj < mHeight; ++jj)
        {
            drawPixelToMemory(pDest, mTiles[ii][jj].RR, mTiles[ii][jj].GG, mTiles[ii][jj].BB);
        }
    }

    mPixelBuffer->unlock();
}

void MiniMap::draw()
{
    // Ogre::Vector3 halfCamera_2dPosition = mCamera_2dPosition / 2;
    for (Ogre::uint ii = 0, mm = mCamera_2dPosition.x - mWidth / (2 * mGrainSize); ii < mWidth; ++mm, ii += mGrainSize)
    {
        for (Ogre::uint jj = 0, nn = mCamera_2dPosition.y - mHeight / (2 * mGrainSize); jj < mHeight; ++nn, jj += mGrainSize)
        {
            // Applying rotation
            int oo = mCamera_2dPosition.x + Helper::round((mm - mCamera_2dPosition.x) * mCosRotation - (nn - mCamera_2dPosition.y) * mSinRotation);
            int pp = mCamera_2dPosition.y + Helper::round((mm - mCamera_2dPosition.x) * mSinRotation + (nn - mCamera_2dPosition.y) * mCosRotation);
            /*FIXME: even if we use a THREE byte pixel format (PF_R8G8B8),
             * for some reason it only works if we have FOUR increments
             * (the empty one is the unused alpha channel)
             * this is not how it is intended/expected
             */
            Tile* tile = mGameMap->getTile(oo, pp);
            if(tile == NULL)
            {
                drawPixel(ii, jj, 0x00, 0x00, 0x00);
                continue;
            }

            switch (tile->getType())
            {
            case Tile::water:
                drawPixel(ii, jj, 0x7F, 0xFF, 0xD4);
                break;

            case Tile::dirt:
                drawPixel(ii, jj, 0x8B, 0x45, 0x13);
                break;

            case Tile::lava:
                drawPixel(ii, jj, 0xB2, 0x22, 0x22);
                break;

            case Tile::rock:
                drawPixel(ii, jj, 0xA9, 0xA9, 0xA9);
                break;

            case Tile::gold:
                drawPixel(ii, jj, 0xFF, 0xD7, 0xD0);
                break;

            case Tile::claimed:
            {
                Seat* tempSeat = tile->getSeat();
                if (tempSeat != NULL)
                {
                    Ogre::ColourValue color = tempSeat->getColorValue();
                    drawPixel(ii, jj, color.r*255.0, color.g*255.0, color.b*255.0);
                }
                else
                {
                    drawPixel(ii, jj, 0x94, 0x00, 0xD3);
                }
                break;
            }

            case Tile::nullTileType:
                drawPixel(ii, jj, 0x00, 0x00, 0x00);
                break;

            default:
                drawPixel(ii,jj,0x00,0xFF,0x7F);
                break;
            }
        }
    }

    // Draw creatures on map.
    // std::vector<Creature*>::iterator updatedCreatureIndex = mGameMap->creatures.begin();
    // for(; updatedCreatureIndex < mGameMap->creatures.end(); ++updatedCreatureIndex)
    // {
    //     if((*updatedCreatureIndex)->getIsOnMap())
    //     {
    //         double  ii = (*updatedCreatureIndex)->getPosition().x;
    //         double  jj = (*updatedCreatureIndex)->getPosition().y;

    //         drawPixel(ii, jj, 0x94, 0x0, 0x94);
    //     }
    // }
}
