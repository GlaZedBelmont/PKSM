/*
 *   This file is part of PKSM
 *   Copyright (C) 2016-2019 Bernardo Giordano, Admiral Fish, piepie62
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
 *       * Requiring preservation of specified reasonable legal notices or
 *         author attributions in that material or in the Appropriate Legal
 *         Notices displayed by works containing it.
 *       * Prohibiting misrepresentation of the origin of that material,
 *         or requiring that modified versions of such material be marked in
 *         reasonable ways as different from the original version.
 */

#include "ReplaceableScreen.hpp"
#include "gui.hpp"

void ReplaceableScreen::dim() const
{
    Gui::drawSolidRect(0, 0, 400, 240, COLOR_MASKBLACK);
}

void ReplaceableScreen::doTopDraw() const
{
    if (overlay)
    {
        if (!overlay->willReplaceTop())
        {
            drawTop();
        }
        overlay->doTopDraw();
    }
    else
    {
        drawTop();
    }
}

void ReplaceableScreen::doBottomDraw() const
{
    if (overlay)
    {
        if (!overlay->willReplaceBottom())
        {
            drawBottom();
        }
        overlay->doBottomDraw();
    }
    else
    {
        drawBottom();
    }
}

void ReplaceableScreen::doUpdate(touchPosition* touch)
{
    if (overlay && overlay->willHandleUpdate())
    {
        overlay->doUpdate(touch);
    }
    else
    {
        update(touch);
    }
}

bool ReplaceableScreen::willHandleUpdate() const
{
    if (overlay)
    {
        return overlay->willHandleUpdate();
    }
    else
    {
        return handlesUpdate();
    }
}

bool ReplaceableScreen::willReplaceBottom() const
{
    if (overlay)
    {
        return overlay->willReplaceBottom();
    }
    else
    {
        return replacesBottom();
    }
}

bool ReplaceableScreen::willReplaceTop() const
{
    if (overlay)
    {
        return overlay->willReplaceTop();
    }
    else
    {
        return replacesTop();
    }
}
