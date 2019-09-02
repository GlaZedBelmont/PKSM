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

#include "gui.hpp"
#include "TextParse.hpp"
#include <queue>

C3D_RenderTarget* g_renderTargetTop;
C3D_RenderTarget* g_renderTargetBottom;
C3D_RenderTarget* g_renderTargetTextChop;
C3D_Tex textChopTexture;

static C2D_SpriteSheet spritesheet_ui;
static C2D_SpriteSheet spritesheet_pkm;
static C2D_SpriteSheet spritesheet_types;
static C2D_Image bgBoxes;
static TextParse::TextBuf* textBuffer;
static TextParse::ScreenText topText;
static TextParse::ScreenText bottomText;
static TextParse::ScreenText* currentText = nullptr;

static std::vector<C2D_Font> fonts;

std::stack<std::unique_ptr<Screen>> screens;

constexpr u32 magicNumber = 0xC7D84AB9;
static float noHomeAlpha  = 0.0f;
#define NOHOMEALPHA_ACCEL 0.001f
static float dNoHomeAlpha = NOHOMEALPHA_ACCEL;

bool textMode = false;
bool inFrame  = false;
bool drawingOnTopScreen;

static int scrollingTextY = 0;
struct ScrollingTextOffset
{
    int offset;
    int pauseTime;
};
static std::unordered_map<std::string, ScrollingTextOffset> scrollOffsets;

static Tex3DS_SubTexture _select_box(const C2D_Image& image, int x, int y, int endX, int endY)
{
    Tex3DS_SubTexture tex = *image.subtex;
    if (x != endX)
    {
        int deltaX  = endX - x;
        float texRL = tex.left - tex.right;
        tex.left    = tex.left - (float)texRL / tex.width * x;
        tex.right   = tex.left - (float)texRL / tex.width * deltaX;
        tex.width   = deltaX;
    }
    if (y != endY)
    {
        float texTB = tex.top - tex.bottom;
        int deltaY  = endY - y;
        tex.top     = tex.top - (float)texTB / tex.height * y;
        tex.bottom  = tex.top - (float)texTB / tex.height * deltaY;
        tex.height  = deltaY;
    }
    return tex;
}

void Gui::drawImageAt(const C2D_Image& img, float x, float y, const C2D_ImageTint* tint, float scaleX, float scaleY)
{
    flushText();
    C2D_DrawImageAt(img, x, y, 0.5f, tint, scaleX, scaleY);
}

void Gui::drawSolidCircle(float x, float y, float rad, u32 color)
{
    flushText();
    C2D_DrawCircleSolid(x, y, 0.5f, rad, color);
}

void Gui::drawSolidRect(float x, float y, float w, float h, u32 color)
{
    flushText();
    C2D_DrawRectSolid(x, y, 0.5f, w, h, color);
}

void Gui::drawSolidTriangle(float x1, float y1, float x2, float y2, float x3, float y3, u32 color)
{
    flushText();
    C2D_DrawTriangle(x1, y1, color, x2, y2, color, x3, y3, color, 0.5f);
}

void Gui::drawLine(float x1, float y1, float x2, float y2, float width, u32 color)
{
    flushText();
    // C2D_DrawLine(x1, y1, x2, y2, 0.5f, width, color);
    float angle = atan2f(y2 - y1, x2 - x1) + C3D_Angle(.25);
    float dy    = width / 2 * sinf(angle);
    float dx    = width / 2 * cosf(angle);
    drawSolidTriangle(x1 - dx, y1 - dy, x1 + dx, y1 + dy, x2 - dx, y2 - dy, color);
    drawSolidTriangle(x2 - dx, y2 - dy, x2 + dx, y2 + dy, x1 + dx, y1 + dy, color);
}

void Gui::setDoHomeDraw()
{
    noHomeAlpha  = 1.0f;
    dNoHomeAlpha = NOHOMEALPHA_ACCEL;
}

void Gui::drawNoHome()
{
    static C2D_ImageTint tint;
    if (noHomeAlpha > 0.0f)
    {
        C2D_AlphaImageTint(&tint, noHomeAlpha);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_home_blocked_idx), 130.0f, 90.0f, &tint);
        noHomeAlpha -= dNoHomeAlpha;
        dNoHomeAlpha += NOHOMEALPHA_ACCEL;
    }
}

void Gui::target(gfxScreen_t screen)
{
    if (screen == GFX_BOTTOM)
    {
        currentText = &bottomText;
        C2D_SceneBegin(g_renderTargetBottom);
    }
    else
    {
        currentText = &topText;
        C2D_SceneBegin(g_renderTargetTop);
    }
    currentText->clear();
}

void Gui::clearScreen(gfxScreen_t screen)
{
    if (screen == GFX_BOTTOM)
    {
        C2D_TargetClear(g_renderTargetBottom, COLOR_BLACK);
    }
    else
    {
        C2D_TargetClear(g_renderTargetTop, COLOR_BLACK);
    }
}

C2D_Image Gui::TWLIcon(void)
{
    return C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_gameselector_twlcart_idx);
}

void Gui::flushText()
{
    if (textMode)
    {
        currentText->optimize();
        currentText->draw();
        currentText->clear();
    }
    textMode = false;
}

void Gui::backgroundBottom(bool stripes)
{
    Gui::drawSolidRect(0, 0, 320, 240, C2D_Color32(40, 53, 147, 255));
    if (stripes)
    {
        for (int x = -240; x < 320; x += 7)
        {
            drawLine(x, 0, x + 240, 240, 2, COLOR_LINEBLUE);
        }
    }
    Gui::drawSolidRect(0, 220, 320, 20, C2D_Color32(26, 35, 126, 255));
}

void Gui::backgroundTop(bool stripes)
{
    Gui::drawSolidRect(0, 0, 400, 240, C2D_Color32(26, 35, 126, 255));
    if (stripes)
    {
        for (int x = -240; x < 400; x += 7)
        {
            drawLine(x, 240, x + 240, 0, 2, COLOR_LINEBLUE);
        }
    }
    Gui::drawSolidRect(0, 0, 400, 25, C2D_Color32(15, 22, 89, 255));
}

void Gui::backgroundAnimatedTop()
{
    static int x1                         = 0;
    static int x2                         = 400;
    static const Tex3DS_SubTexture boxes1 = _select_box(bgBoxes, 0, 0, 400, 240);
    static const Tex3DS_SubTexture boxes2 = _select_box(bgBoxes, 400, 0, 800, 240);

    if (x1 < -400)
    {
        x1 = 400;
    }
    if (x2 < -400)
    {
        x2 = 400;
    }

    Gui::drawImageAt({bgBoxes.tex, &boxes1}, x1--, 0);
    Gui::drawImageAt({bgBoxes.tex, &boxes2}, x2--, 0);
}

void Gui::backgroundAnimatedBottom()
{
    static int x1                         = 0;
    static int x2                         = 400;
    static const Tex3DS_SubTexture boxes1 = _select_box(bgBoxes, 0, 0, 400, 240);
    static const Tex3DS_SubTexture boxes2 = _select_box(bgBoxes, 400, 0, 800, 240);

    if (x1 < -400)
    {
        x1 = 400;
    }
    if (x2 < -400)
    {
        x2 = 400;
    }

    Gui::drawImageAt({bgBoxes.tex, &boxes1}, x1--, 0);
    Gui::drawImageAt({bgBoxes.tex, &boxes2}, x2--, 0);
}

void Gui::clearText(void)
{
    textBuffer->clearUnconditional();
}

std::shared_ptr<TextParse::Text> Gui::parseText(const std::string& str, float scaleX, float maxWidth)
{
    maxWidth /= scaleX;
    return textBuffer->parse(str, maxWidth);
}

void Gui::text(std::shared_ptr<TextParse::Text> text, float x, float y, float scaleX, float scaleY, u32 color, TextPosX positionX, TextPosY positionY)
{
    textMode            = true;
    const float lineMod = scaleY * C2D_FontGetInfo(fonts[1])->lineFeed;
    y -= scaleY * 6;
    switch (positionY)
    {
        case TextPosY::TOP:
            break;
        case TextPosY::CENTER:
            y -= 0.5f * (lineMod * (float)text->lineWidths.size());
            break;
        case TextPosY::BOTTOM:
            y -= lineMod * (float)text->lineWidths.size();
            break;
    }

    currentText->addText(text, x, y, 0.5f, scaleX, scaleY, positionX, color);
}

void Gui::text(
    const std::string& str, float x, float y, float scaleX, float scaleY, u32 color, TextPosX positionX, TextPosY positionY, float maxWidth)
{
    auto text = parseText(str, scaleX, maxWidth);

    Gui::text(text, x, y, scaleX, scaleY, color, positionX, positionY);
}

void Gui::scrollingText(
    const std::string& str, float x, float y, float scaleX, float scaleY, u32 color, TextPosX positionX, TextPosY positionY, int width)
{
    static const Tex3DS_SubTexture t3x = {512, 256, 0.0f, 1.0f, 1.0f, 0.0f};
    static const C2D_Image textImage   = {&textChopTexture, &t3x};

    auto text = parseText(str, scaleX);
    text->optimize();
    if (!scrollOffsets.count(str))
    {
        scrollOffsets[str] = {0, 1};
    }

    static const float lineMod     = scaleY * C2D_FontGetInfo(nullptr)->lineFeed;
    static const float baselinePos = scaleY * C2D_FontGetInfo(nullptr)->tglp->baselinePos;
    y -= scaleY * 6;
    switch (positionY)
    {
        case TextPosY::TOP:
            break;
        case TextPosY::CENTER:
            y -= 0.5f * (lineMod * (float)text->lineWidths.size());
            break;
        case TextPosY::BOTTOM:
            y -= lineMod * (float)text->lineWidths.size();
            break;
    }

    C2D_SceneBegin(g_renderTargetTextChop);
    text->draw(-scrollOffsets[str].offset / 3, scrollingTextY, 0, scaleX, scaleY, positionX, color);
    C2D_SceneBegin(drawingOnTopScreen ? g_renderTargetTop : g_renderTargetBottom);
    Tex3DS_SubTexture newt3x = _select_box(textImage, 0, scrollingTextY + lineMod - baselinePos, width, scrollingTextY + lineMod * 2 - baselinePos);
    scrollingTextY += ceilf(lineMod);
    if (scrollOffsets[str].pauseTime != 0)
    {
        if (scrollOffsets[str].pauseTime > 30)
        {
            if (scrollOffsets[str].offset == 0)
            {
                scrollOffsets[str].pauseTime = 0;
            }
            else
            {
                scrollOffsets[str].pauseTime = 1;
            }
            scrollOffsets[str].offset = 0;
        }
        else
        {
            scrollOffsets[str].pauseTime++;
        }
    }
    else
    {
        scrollOffsets[str].offset += 1;
        if (scrollOffsets[str].offset / 3 + width > (int)text->maxLineWidth * scaleX + 5)
        {
            scrollOffsets[str].pauseTime += 1;
        }
    }
    C2D_DrawImageAt({&textChopTexture, &newt3x}, x, y + lineMod - baselinePos, 0.5f);
}

void Gui::slicedText(
    const std::string& str, float x, float y, float scaleX, float scaleY, u32 color, TextPosX positionX, TextPosY positionY, int width)
{
    static const Tex3DS_SubTexture t3x = {512, 256, 0.0f, 1.0f, 1.0f, 0.0f};
    static const C2D_Image textImage   = {&textChopTexture, &t3x};

    auto text = parseText(str, scaleX);
    text->optimize();

    static const float lineMod     = scaleY * C2D_FontGetInfo(nullptr)->lineFeed;
    static const float baselinePos = scaleY * C2D_FontGetInfo(nullptr)->tglp->baselinePos;
    y -= scaleY * 6;
    switch (positionY)
    {
        case TextPosY::TOP:
            break;
        case TextPosY::CENTER:
            y -= 0.5f * (lineMod * (float)text->lineWidths.size());
            break;
        case TextPosY::BOTTOM:
            y -= lineMod * (float)text->lineWidths.size();
            break;
    }

    C2D_SceneBegin(g_renderTargetTextChop);
    text->draw(0, scrollingTextY, 0, scaleX, scaleY, positionX, color);
    C2D_SceneBegin(drawingOnTopScreen ? g_renderTargetTop : g_renderTargetBottom);
    Tex3DS_SubTexture newt3x = _select_box(textImage, 0, scrollingTextY + lineMod - baselinePos, width, scrollingTextY + lineMod * 2 - baselinePos);
    scrollingTextY += ceilf(lineMod);
    scrollOffsets[str] = {0, 1};
    C2D_DrawImageAt({&textChopTexture, &newt3x}, x, y + lineMod - baselinePos, 0.5f);
}

static void _draw_mirror_scale(int key, int x, int y, int off, int rep)
{
    C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
    // Left side
    Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
    Gui::drawImageAt({sprite.tex, &tex}, x, y);
    // Right side
    Gui::drawImageAt({sprite.tex, &tex}, x + off + rep, y, nullptr, -1.0f, 1.0f);
    // Center
    tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
    Gui::drawImageAt({sprite.tex, &tex}, x + off, y, nullptr, rep, 1.0f);
}

static void _draw_repeat(int key, int x, int y, u8 rows, u8 cols)
{
    C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
    for (u8 row = 0; row < rows; row++)
    {
        for (u8 col = 0; col < cols; col++)
        {
            Gui::drawImageAt(sprite, x + col * sprite.subtex->width, y + row * sprite.subtex->height);
        }
    }
}

Result Gui::init(void)
{
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    SDLH_Init();

    g_renderTargetTop    = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    g_renderTargetBottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    C3D_TexInitVRAM(&textChopTexture, 512, 256, GPU_RGBA8);
    g_renderTargetTextChop = C3D_RenderTargetCreateFromTex(&textChopTexture, GPU_TEXFACE_2D, 0, GPU_RB_DEPTH16);

    spritesheet_ui    = C2D_SpriteSheetLoad("romfs:/gfx/ui_sheet.t3x");
    spritesheet_pkm   = C2D_SpriteSheetLoad("/3ds/PKSM/assets/pkm_spritesheet.t3x");
    spritesheet_types = C2D_SpriteSheetLoad("/3ds/PKSM/assets/types_spritesheet.t3x");

    fonts.emplace_back(C2D_FontLoad("romfs:/gfx/pksm.bcfnt"));
    fonts.emplace_back(C2D_FontLoadSystem(CFG_REGION_USA));
    fonts.emplace_back(C2D_FontLoadSystem(CFG_REGION_KOR));
    fonts.emplace_back(C2D_FontLoadSystem(CFG_REGION_CHN));
    fonts.emplace_back(C2D_FontLoadSystem(CFG_REGION_TWN));

    textBuffer = new TextParse::TextBuf(8192, fonts);

    bgBoxes = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_anim_squares_idx);

    return 0;
}

// From sound.cpp. Not in sound.hpp for an implementation-independent sound.hpp
extern void SOUND_correctBGMDataSize();

void Gui::mainLoop(void)
{
    bool exit = false;
    Threads::create((ThreadFunc)SDLH_Play);
    while (aptMainLoop() && !exit)
    {
        hidScanInput();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        inFrame = true;
        Gui::clearScreen(GFX_TOP);
        Gui::clearScreen(GFX_BOTTOM);
        C2D_TargetClear(g_renderTargetTextChop, 0);
        scrollingTextY = 0;

        u32 kHeld = hidKeysHeld();
        if (kHeld & KEY_SELECT && !screens.top()->getInstructions().empty())
        {
            target(GFX_TOP);
            screens.top()->doTopDraw();
            screens.top()->getInstructions().drawTop();
            flushText();
            target(GFX_BOTTOM);
            screens.top()->doBottomDraw();
            screens.top()->getInstructions().drawBottom();
            flushText();

            if (!aptIsHomeAllowed() && aptIsHomePressed())
            {
                setDoHomeDraw();
            }
            drawNoHome();

            C3D_FrameEnd(0);
            inFrame = false;
        }
        else
        {
            target(GFX_TOP);
            screens.top()->doTopDraw();
            flushText();

            target(GFX_BOTTOM);
            screens.top()->doBottomDraw();
            flushText();

            if (!aptIsHomeAllowed() && aptIsHomePressed())
            {
                setDoHomeDraw();
            }
            drawNoHome();

            C3D_FrameEnd(0);
            inFrame = false;

            touchPosition touch;
            hidTouchRead(&touch);
            screens.top()->doUpdate(&touch);
            exit = screens.size() == 1 && (kHeld & KEY_START);
        }

        textBuffer->clear();
        SOUND_correctBGMDataSize();
    }
}

void Gui::exit(void)
{
    if (spritesheet_ui)
    {
        C2D_SpriteSheetFree(spritesheet_ui);
    }
    if (spritesheet_pkm)
    {
        C2D_SpriteSheetFree(spritesheet_pkm);
    }
    if (spritesheet_types)
    {
        C2D_SpriteSheetFree(spritesheet_types);
    }
    if (textBuffer)
    {
        delete textBuffer;
    }
    for (auto font : fonts)
    {
        if (font)
            C2D_FontFree(font);
    }
    C2D_Fini();
    C3D_Fini();
    SDLH_Exit();
}

void Gui::sprite(int key, int x, int y)
{
    if (key == ui_sheet_res_null_idx)
    {
        return;
    }
    else if (key == ui_sheet_eventmenu_bar_selected_idx || key == ui_sheet_eventmenu_bar_unselected_idx)
    {
        u8 off = 4, rep = 174;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        // Left side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x, y);
        // Center
        tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + off, y, nullptr, rep, 1.0f);
    }
    else if (key == ui_sheet_eventmenu_page_indicator_idx)
    {
        _draw_mirror_scale(key, x, y, 4, 182);
    }
    else if (key == ui_sheet_gameselector_savebox_idx)
    {
        u8 off           = 53;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        // Top side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, 0, off);
        Gui::drawImageAt({sprite.tex, &tex}, x, y);
        // Bottom side
        Gui::drawImageAt({sprite.tex, &tex}, x, y + off - 1, nullptr, 1.0f, -1.0f);
    }
    else if (key == ui_sheet_mainmenu_button_idx)
    {
        _draw_mirror_scale(key, x, y, 4, 132);
    }
    else if (key == ui_sheet_part_editor_20x2_idx)
    {
        _draw_repeat(key, x, y, 20, 1);
    }
    else if (key == ui_sheet_part_info_bottom_idx || key == ui_sheet_part_info_top_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        int width = sprite.subtex->width, height = sprite.subtex->height;
        // Top left
        Gui::drawImageAt(sprite, x, y);
        // Top right
        Gui::drawImageAt(sprite, x + width, y, NULL, -1.0f, 1.0f);
        // Bottom left
        Gui::drawImageAt(sprite, x, y + height, NULL, 1.0f, -1.0f);
        // Bottom right
        Gui::drawImageAt(sprite, x + width, y + height, NULL, -1.0f, -1.0f);
    }
    else if (key == ui_sheet_part_mtx_4x4_idx)
    {
        _draw_repeat(key, x, y, 4, 4);
    }
    else if (key == ui_sheet_part_mtx_5x6_idx)
    {
        _draw_repeat(key, x, y, 5, 6);
    }
    else if (key == ui_sheet_part_mtx_5x8_idx)
    {
        _draw_repeat(key, x, y, 5, 8);
    }
    else if (key == ui_sheet_part_mtx_15x16_idx)
    {
        _draw_repeat(key, x, y, 15, 16);
        Gui::drawSolidRect(0, 225, 400, 15, COLOR_WHITE);
    }

    // emulated
    else if (key == ui_sheet_emulated_pointer_horizontal_flipped_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_pointer_horizontal_idx);
        Gui::drawImageAt(sprite, x, y, NULL, -1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_bg_top_red)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(228, 143, 131, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(201, 95, 84, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(239, 163, 151, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(214, 117, 106, 255), 1);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_top_greyscale_idx), x, y, &tint);
    }
    else if (key == ui_sheet_emulated_bg_top_blue)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(139, 171, 221, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(93, 134, 193, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(158, 186, 233, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(113, 150, 205, 255), 1);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_top_greyscale_idx), x, y, &tint);
    }
    else if (key == ui_sheet_emulated_bg_top_green)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(142, 221, 138, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(101, 193, 93, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(161, 233, 158, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(119, 205, 113, 255), 1);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_top_greyscale_idx), x, y, &tint);
    }
    else if (key == ui_sheet_emulated_bg_bottom_red)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(216, 122, 111, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(239, 163, 151, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(201, 95, 84, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(224, 134, 123, 255), 1);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_bottom_greyscale_idx), x, y, &tint);
    }
    else if (key == ui_sheet_emulated_bg_bottom_blue)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(120, 154, 209, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(158, 186, 233, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(93, 134, 193, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(131, 165, 217, 255), 1);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_bottom_greyscale_idx), x, y, &tint);
    }
    else if (key == ui_sheet_emulated_bg_bottom_green)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(125, 209, 119, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(161, 233, 158, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(101, 193, 93, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(136, 217, 131, 255), 1);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_bottom_greyscale_idx), x, y, &tint);
    }
    else if (key == ui_sheet_emulated_eventmenu_bar_selected_flipped_horizontal_idx)
    {
        u8 off = 4, rep = 174;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_eventmenu_bar_selected_idx);
        // Right side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + rep, y, nullptr, -1.0f, 1.0f);
        // Center
        tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x, y, nullptr, -rep, 1.0f);
    }
    else if (key == ui_sheet_emulated_eventmenu_bar_selected_flipped_vertical_idx)
    {
        u8 off = 4, rep = 174;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_eventmenu_bar_selected_idx);
        // Left side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x, y, nullptr, 1.0f, -1.0f);
        // Center
        tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + off, y, nullptr, rep, -1.0f);
    }
    else if (key == ui_sheet_emulated_eventmenu_bar_selected_flipped_both_idx)
    {
        u8 off = 4, rep = 174;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_eventmenu_bar_selected_idx);
        // Right side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + rep, y, nullptr, -1.0f, -1.0f);
        // Center
        tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x, y, nullptr, -rep, -1.0f);
    }
    else if (key == ui_sheet_emulated_eventmenu_bar_unselected_flipped_horizontal_idx)
    {
        u8 off = 4, rep = 174;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_eventmenu_bar_unselected_idx);
        // Right side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + rep, y, nullptr, -1.0f, 1.0f);
        // Center
        tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x, y, nullptr, -rep, 1.0f);
    }
    else if (key == ui_sheet_emulated_eventmenu_bar_unselected_flipped_vertical_idx)
    {
        u8 off = 4, rep = 174;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_eventmenu_bar_unselected_idx);
        // Left side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x, y, nullptr, 1.0f, -1.0f);
        // Center
        tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + off, y, nullptr, rep, -1.0f);
    }
    else if (key == ui_sheet_emulated_eventmenu_bar_unselected_flipped_both_idx)
    {
        u8 off = 4, rep = 174;
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_eventmenu_bar_unselected_idx);
        // Right side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, off, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + rep, y, nullptr, -1.0f, -1.0f);
        // Center
        tex = _select_box(sprite, off, 0, sprite.subtex->width, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x, y, nullptr, -rep, -1.0f);
    }
    else if (key == ui_sheet_emulated_storage_box_corner_flipped_horizontal_idx)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_storage_box_corner_idx), x, y, nullptr, -1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_storage_box_corner_flipped_vertical_idx)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_storage_box_corner_idx), x, y, nullptr, 1.0f, -1.0f);
    }
    else if (key == ui_sheet_emulated_storage_box_corner_flipped_both_idx)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_storage_box_corner_idx), x, y, nullptr, -1.0f, -1.0f);
    }
    else if (key == ui_sheet_emulated_toggle_green_idx)
    {
        Gui::drawSolidRect(x, y, 13, 13, C2D_Color32(0x35, 0xC1, 0x3E, 0xFF));
    }
    else if (key == ui_sheet_emulated_toggle_red_idx)
    {
        Gui::drawSolidRect(x, y, 13, 13, C2D_Color32(0xCC, 0x3F, 0x26, 0xFF));
    }
    else if (key == ui_sheet_emulated_gameselector_bg_idx)
    {
        u8 off = 5, rep = 197;
        /* LEFT */
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_gameselector_bg_left_idx);
        // Top side
        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, 0, off);
        Gui::drawImageAt({sprite.tex, &tex}, x, y);
        // Bottom side
        Gui::drawImageAt({sprite.tex, &tex}, x, y + off + rep, nullptr, 1.0f, -1.0f);
        // Center
        tex = _select_box(sprite, 0, off, 0, sprite.subtex->height);
        Gui::drawImageAt({sprite.tex, &tex}, x, y + off, nullptr, 1.0f, rep);
        x += 5;
        Gui::drawSolidRect(x, y, 115, rep + 10, C2D_Color32(26, 35, 126, 255));

        /* RIGHT */
        x += 119;
        Gui::drawSolidRect(x, y, 263, rep + 10, C2D_Color32(26, 35, 126, 255));
        x += 263;
        sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_gameselector_bg_left_idx);
        // Top side
        tex = _select_box(sprite, 0, 0, 0, off);
        Gui::drawImageAt({sprite.tex, &tex}, x, y, nullptr, -1.0f, 1.0f);
        // Bottom side
        Gui::drawImageAt({sprite.tex, &tex}, x, y + off + rep, nullptr, -1.0f, -1.0f);
        // Center
        tex = _select_box(sprite, 0, off, 0, sprite.subtex->height);
        Gui::drawImageAt({sprite.tex, &tex}, x, y + off, nullptr, 1.0f, rep);
    }
    else if (key == ui_sheet_emulated_button_qr_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_greyscale_idx);
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_SELECTBLUE, 1.0f);

        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, 5, 0);
        // Left
        Gui::drawImageAt({sprite.tex, &tex}, x, y, &tint, 1.0f, 1.0f);
        // Right
        Gui::drawImageAt({sprite.tex, &tex}, x + 65, y, &tint, -1.0f, 1.0f);
        // Middle
        tex = _select_box(sprite, 5, 0, 6, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + 5, y, &tint, 60.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_item_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_editor_idx);

        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, 16, 0);
        // Left
        Gui::drawImageAt({sprite.tex, &tex}, x, y);
        // Right
        tex = _select_box(sprite, 92, 0, 108, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + 182, y);
        // Center
        tex = _select_box(sprite, 16, 0, 17, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + 16, y, nullptr, 166.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_plus_small_black_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_BLACK, 1.0f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_plus_small_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_minus_small_black_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_BLACK, 1.0f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_minus_small_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_box_search_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_greyscale_idx);
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_SELECTBLUE, 1.0f);

        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, 5, 0);
        // Left
        Gui::drawImageAt({sprite.tex, &tex}, x, y, &tint, 1.0f, 1.0f);
        // Right
        Gui::drawImageAt({sprite.tex, &tex}, x + 165, y, &tint, -1.0f, 1.0f);
        // Middle
        tex = _select_box(sprite, 5, 0, 6, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + 5, y, &tint, 160.0f, 1.0f);

        Gui::drawSolidRect(x + 20, y + 17, 144, 1, COLOR_WHITE);
    }
    else if (key == ui_sheet_emulated_toggle_gray_idx)
    {
        Gui::drawSolidRect(x, y, 13, 13, C2D_Color32(0x80, 0x80, 0x80, 0xFF));
    }
    else if (key == ui_sheet_emulated_toggle_blue_idx)
    {
        Gui::drawSolidRect(x, y, 13, 13, C2D_Color32(0x00, 0x00, 0xFF, 0xFF));
    }
    else if (key == ui_sheet_emulated_party_indicator_1_idx)
    {
        text("\u2460", x, y - 3, FONT_SIZE_12, FONT_SIZE_12, COLOR_BLACK, TextPosX::LEFT, TextPosY::TOP);
    }
    else if (key == ui_sheet_emulated_party_indicator_2_idx)
    {
        text("\u2461", x, y - 3, FONT_SIZE_12, FONT_SIZE_12, COLOR_BLACK, TextPosX::LEFT, TextPosY::TOP);
    }
    else if (key == ui_sheet_emulated_party_indicator_3_idx)
    {
        text("\u2462", x, y - 3, FONT_SIZE_12, FONT_SIZE_12, COLOR_BLACK, TextPosX::LEFT, TextPosY::TOP);
    }
    else if (key == ui_sheet_emulated_party_indicator_4_idx)
    {
        text("\u2463", x, y - 3, FONT_SIZE_12, FONT_SIZE_12, COLOR_BLACK, TextPosX::LEFT, TextPosY::TOP);
    }
    else if (key == ui_sheet_emulated_party_indicator_5_idx)
    {
        text("\u2464", x, y - 3, FONT_SIZE_12, FONT_SIZE_12, COLOR_BLACK, TextPosX::LEFT, TextPosY::TOP);
    }
    else if (key == ui_sheet_emulated_party_indicator_6_idx)
    {
        text("\u2465", x, y - 3, FONT_SIZE_12, FONT_SIZE_12, COLOR_BLACK, TextPosX::LEFT, TextPosY::TOP);
    }
    else if (key == ui_sheet_emulated_button_selected_blue_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_SELECTBLUE, 1.0f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_greyscale_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_unselected_blue_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_UNSELECTBLUE, 1.0f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_greyscale_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_unavailable_blue_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_UNAVAILBLUE, 1.0f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_greyscale_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_selected_red_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_SELECTRED, 1.0f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_greyscale_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_unselected_red_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_UNSELECTRED, 1.0f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_greyscale_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_unavailable_red_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_UNAVAILRED, 1.0f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_greyscale_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_button_pouch_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_editor_idx);

        Tex3DS_SubTexture tex = _select_box(sprite, 0, 0, 16, 0);
        // Left
        Gui::drawImageAt({sprite.tex, &tex}, x, y);
        // Right
        tex = _select_box(sprite, 92, 0, 108, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + 84, y);
        // Center
        tex = _select_box(sprite, 16, 0, 17, 0);
        Gui::drawImageAt({sprite.tex, &tex}, x + 16, y, nullptr, 68.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_textbox_illegal_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(0xFF, 0, 0, 0xFF), 0.1f);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_textbox_pksm_idx), x, y, &tint, 1.0f, 1.0f);
    }
    else if (key == ui_sheet_emulated_bg_top_yellow_idx)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(242, 221, 131, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(239, 202, 43, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(246, 230, 158, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(244, 212, 81, 255), 1);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_top_greyscale_idx), x, y, 0.5f, &tint);
    }
    else if (key == ui_sheet_emulated_bg_bottom_yellow_idx)
    {
        C2D_ImageTint tint;
        C2D_SetImageTint(&tint, C2D_TopLeft, C2D_Color32(244, 220, 118, 255), 1);
        C2D_SetImageTint(&tint, C2D_TopRight, C2D_Color32(246, 230, 158, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotLeft, C2D_Color32(242, 211, 78, 255), 1);
        C2D_SetImageTint(&tint, C2D_BotRight, C2D_Color32(242, 221, 131, 255), 1);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_bg_bottom_greyscale_idx), x, y, 0.5f, &tint);
    }
    else if (key == ui_sheet_emulated_button_lang_disabled_idx)
    {
        drawSolidRect(x, y, 8, 8, COLOR_MENUBLUE);
    }
    else if (key == ui_sheet_emulated_button_lang_enabled_idx)
    {
        drawSolidRect(x - 1, y - 1, 10, 10, COLOR_HIGHBLUE);
    }
    else if (key == ui_sheet_emulated_stripe_move_grey_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, COLOR_DARKGREY, 1.0f);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_stripe_move_editor_row_idx), x, y, 0.5f, &tint);
    }
    else if (key == ui_sheet_emulated_button_filter_positive_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(0x10, 0x87, 0x1e, 255), 1.0f);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_plus_small_idx), x, y, 0.5f, &tint);
    }
    else if (key == ui_sheet_emulated_button_filter_negative_idx)
    {
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32(0xbd, 0x30, 0x26, 255), 1.0f);
        C2D_DrawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_button_minus_small_idx), x, y, 0.5f, &tint);
    }
    else if (key == ui_sheet_emulated_button_tab_unselected_idx)
    {
        drawSolidRect(x, y, 104, 17, COLOR_DARKBLUE);
    }
    // standard case
    else
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, key), x, y);
    }
}

void Gui::format(const PKX& pkm, int x, int y)
{
    switch (pkm.generation())
    {
            // case 1: // sapphire
            // case 2: // ruby
            // case 3: // emerald
            // case 4: // fire red
            // case 5: // leaf green
            // case 15: // colosseum/XD
            // 	Gui::sprite(ui_sheet_icon_generation_3_idx, x, y);
            //     break;
        case Generation::FOUR:
            Gui::sprite(ui_sheet_icon_generation_4_idx, x, y);
            break;
        case Generation::FIVE:
            Gui::sprite(ui_sheet_icon_generation_5_idx, x, y);
            break;
        case Generation::SIX:
            Gui::sprite(ui_sheet_icon_generation_6_idx, x, y);
            break;
        case Generation::SEVEN:
            Gui::sprite(ui_sheet_icon_generation_7_idx, x, y);
            break;
        case Generation::LGPE:
            Gui::sprite(ui_sheet_icon_generation_go_idx, x, y);
            break;
            // case 35: // rd
            // case 36: // gn
            // case 37: // bu
            // case 38: // yw
            // case 39: // gd
            // case 40: // sv
            // 	Gui::sprite(ui_sheet_icon_generation_gb_idx, x, y);
            //     break;
        default:
            break;
    }
}

void Gui::generation(const PKX& pkm, int x, int y)
{
    switch (pkm.version())
    {
        case 1:  // sapphire
        case 2:  // ruby
        case 3:  // emerald
        case 4:  // fire red
        case 5:  // leaf green
        case 15: // colosseum/XD
            Gui::sprite(ui_sheet_icon_generation_3_idx, x, y);
            break;
        case 10: // diamond
        case 11: // pearl
        case 12: // platinum
        case 7:  // heart gold
        case 8:  // soul silver
            Gui::sprite(ui_sheet_icon_generation_4_idx, x, y);
            break;
        case 20: // white
        case 21: // black
        case 22: // white2
        case 23: // black2
            Gui::sprite(ui_sheet_icon_generation_5_idx, x, y);
            break;
        case 24: // x
        case 25: // y
        case 26: // as
        case 27: // or
            Gui::sprite(ui_sheet_icon_generation_6_idx, x, y);
            break;
        case 30: // sun
        case 31: // moon
        case 32: // us
        case 33: // um
            Gui::sprite(ui_sheet_icon_generation_7_idx, x, y);
            break;
        case 34: // go
            Gui::sprite(ui_sheet_icon_generation_go_idx, x, y);
            break;
        case 35: // rd
        case 36: // gn
        case 37: // bu
        case 38: // yw
        case 39: // gd
        case 40: // sv
        case 41: // cr
            Gui::sprite(ui_sheet_icon_generation_gb_idx, x, y);
            break;
        default:
            break;
    }
}

void Gui::sprite(int key, int x, int y, u32 color)
{
    if (key == ui_sheet_button_editor_idx || key == ui_sheet_icon_item_idx || key == ui_sheet_pointer_arrow_idx)
    {
        C2D_Image sprite = C2D_SpriteSheetGetImage(spritesheet_ui, key);
        C2D_ImageTint tint;
        for (int i = 0; i < 4; i++)
        {
            tint.corners[i] = {color, 1.0f};
        }
        Gui::drawImageAt(sprite, x, y, &tint);
    }
}

void Gui::pkm(const PKX& pokemon, int x, int y, float scale, u32 color, float blend)
{
    static C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, blend);

    if (pokemon.egg())
    {
        if (pokemon.species() != 490)
        {
            pkm(pokemon.species(), pokemon.alternativeForm(), pokemon.generation(), pokemon.gender(), x, y, scale, color, blend);
            Gui::drawImageAt(
                C2D_SpriteSheetGetImage(spritesheet_pkm, pkm_spritesheet_0_idx), x - 13 + ceilf(3 * scale), y + 4 + 30 * (scale - 1), &tint);
        }
        else
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_490_e_idx), x, y, &tint, scale, scale);
        }
    }
    else
    {
        pkm(pokemon.species(), pokemon.alternativeForm(), pokemon.generation(), pokemon.gender(), x, y, scale, color, blend);
        if (pokemon.heldItem() > 0)
        {
            Gui::drawImageAt(
                C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_icon_item_idx), x + ceilf(3 * scale), y + 21 + ceilf(30 * (scale - 1)), &tint);
        }
    }

    if (pokemon.shiny())
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_icon_shiny_idx), x, y, &tint);
    }
}

void Gui::pkm(int species, int form, Generation generation, int gender, int x, int y, float scale, u32 color, float blend)
{
    static C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, color, blend);
    time_t thing = time(NULL);
    if (gmtime(&thing)->tm_mday == ((u16)(~magicNumber >> 16) ^ 0x3826) && gmtime(&thing)->tm_mon == ((u16)(~magicNumber) ^ 0xB545))
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, (u8)(~magicNumber >> 8)), x, y, &tint, scale, scale);
        return;
    }
    if (species == 490 && form == -1)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_490_e_idx), x, y, &tint, scale, scale);
    }
    else if (species == 201)
    {
        if (form == 0 || form > 27)
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
        }
        else
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_801_1_idx + form), x, y, &tint, scale, scale);
        }
    }
    // For possible hex editor mishaps
    else if (species > 809)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, pkm_spritesheet_0_idx), x, y, &tint, scale, scale);
    }
    // Unfezant
    else if (species == 521 && gender == 1)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_521_1_idx), x, y, &tint, scale, scale);
    }
    // Unfezant
    else if (species == 592 && gender == 1)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_592_1_idx), x, y, &tint, scale, scale);
    }
    // Unfezant
    else if (species == 593 && gender == 1)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_593_1_idx), x, y, &tint, scale, scale);
    }
    // Unfezant
    else if (species == 668 && gender == 1)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_668_1_idx), x, y, &tint, scale, scale);
    }
    else if (form == 0)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
    }
    // Mimikyu
    else if (species == 778)
    {
        if (form == 1 || form > PersonalSMUSUM::formCount(778))
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, pkm_spritesheet_778_idx), x, y, &tint, scale, scale);
        }
        else
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_778_2_idx), x, y, &tint, scale, scale);
        }
    }
    // Minior
    else if (species == 774)
    {
        if (form < 7 || form > PersonalSMUSUM::formCount(774))
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, pkm_spritesheet_774_idx), x, y, &tint, scale, scale);
        }
        else
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_774_7_idx + form - 7), x, y, &tint, scale, scale);
        }
    }
    // Pumpkaboo, Gourgeist, & Genesect
    else if (species == 710 || species == 711 || species == 649)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
    }
    // Pikachu
    else if (species == 25)
    {
        if (generation == Generation::SIX && form < PersonalXYORAS::formCount(25))
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_20_2_idx + form), x, y, &tint, scale, scale);
        }
        else if (form < PersonalSMUSUM::formCount(25))
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_25_6_idx + form), x, y, &tint, scale, scale);
        }
        else if (form == PersonalLGPE::formCount(25) - 1)
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_icon_shiny_idx), x + 25 + 34 * (scale - 1), y + 5);
        }
        else
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
        }
    }
    // LGPE starter Eevee
    else if (species == 133)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_ui, ui_sheet_icon_shiny_idx), x + 25 + 34 * (scale - 1), y + 5);
    }
    // Arceus
    else if (species == 493)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
    }
    // Vivillon chain
    else if (species == 664 || species == 665)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
    }
    // Silvally
    else if (species == 773)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
    }
    else
    {
        if (form > PersonalLGPE::formCount(species))
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, species), x, y, &tint, scale, scale);
            return;
        }
        int imageOffsetFromBack = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
        switch (species)
        {
            // case NEXT_SPECIES_WITH_FORMS:
            // imageOffsetFromBack += 1;
            case 801:
                imageOffsetFromBack += 3;
            case 800:
                imageOffsetFromBack += 1;
            case 784:
                imageOffsetFromBack += 1;
            case 778:
                imageOffsetFromBack += 1;
            case 777:
                imageOffsetFromBack += 7;
            case 774:
                imageOffsetFromBack += 1;
            case 758:
                imageOffsetFromBack += 1;
            case 754:
                imageOffsetFromBack += 1;
            case 752:
                imageOffsetFromBack += 1;
            case 746:
                imageOffsetFromBack += 2;
            case 745:
                imageOffsetFromBack += 1;
            case 744:
                imageOffsetFromBack += 1;
            case 743:
                imageOffsetFromBack += 3;
            case 741:
                imageOffsetFromBack += 1;
            case 738:
                imageOffsetFromBack += 1;
            case 735:
                imageOffsetFromBack += 1;
            case 720:
                imageOffsetFromBack += 1;
            case 719:
                imageOffsetFromBack += 4;
            case 718:
                imageOffsetFromBack += 1;
            case 681:
                imageOffsetFromBack += 1;
            case 678:
                imageOffsetFromBack += 9;
            case 676:
                imageOffsetFromBack += 4;
            case 671:
                imageOffsetFromBack += 5;
            case 670:
                imageOffsetFromBack += 4;
            case 669:
                imageOffsetFromBack += 19;
            case 666:
                imageOffsetFromBack += 2;
            case 658:
                imageOffsetFromBack += 1;
            case 648:
                imageOffsetFromBack += 1;
            case 647:
                imageOffsetFromBack += 2;
            case 646:
                imageOffsetFromBack += 1;
            case 645:
                imageOffsetFromBack += 1;
            case 642:
                imageOffsetFromBack += 1;
            case 641:
                imageOffsetFromBack += 3;
            case 586:
                imageOffsetFromBack += 3;
            case 585:
                imageOffsetFromBack += 1;
            case 555:
                imageOffsetFromBack += 1;
            case 550:
                imageOffsetFromBack += 1;
            case 531:
                imageOffsetFromBack += 1;
            case 492:
                imageOffsetFromBack += 1;
            case 487:
                imageOffsetFromBack += 5;
            case 479:
                imageOffsetFromBack += 1;
            case 475:
                imageOffsetFromBack += 1;
            case 460:
                imageOffsetFromBack += 1;
            case 448:
                imageOffsetFromBack += 1;
            case 445:
                imageOffsetFromBack += 1;
            case 428:
                imageOffsetFromBack += 1;
            case 423:
                imageOffsetFromBack += 1;
            case 422:
                imageOffsetFromBack += 1;
            case 421:
                imageOffsetFromBack += 2;
            case 413:
                imageOffsetFromBack += 2;
            case 412:
                imageOffsetFromBack += 3;
            case 386:
                imageOffsetFromBack += 1;
            case 384:
                imageOffsetFromBack += 1;
            case 383:
                imageOffsetFromBack += 1;
            case 382:
                imageOffsetFromBack += 1;
            case 381:
                imageOffsetFromBack += 1;
            case 380:
                imageOffsetFromBack += 1;
            case 376:
                imageOffsetFromBack += 1;
            case 373:
                imageOffsetFromBack += 1;
            case 362:
                imageOffsetFromBack += 1;
            case 359:
                imageOffsetFromBack += 1;
            case 354:
                imageOffsetFromBack += 3;
            case 351:
                imageOffsetFromBack += 1;
            case 334:
                imageOffsetFromBack += 1;
            case 323:
                imageOffsetFromBack += 1;
            case 319:
                imageOffsetFromBack += 1;
            case 310:
                imageOffsetFromBack += 1;
            case 308:
                imageOffsetFromBack += 1;
            case 306:
                imageOffsetFromBack += 1;
            case 303:
                imageOffsetFromBack += 1;
            case 302:
                imageOffsetFromBack += 1;
            case 282:
                imageOffsetFromBack += 1;
            case 260:
                imageOffsetFromBack += 1;
            case 257:
                imageOffsetFromBack += 1;
            case 254:
                imageOffsetFromBack += 1;
            case 248:
                imageOffsetFromBack += 1;
            case 229:
                imageOffsetFromBack += 1;
            case 214:
                imageOffsetFromBack += 1;
            case 212:
                imageOffsetFromBack += 1;
            case 208:
                imageOffsetFromBack += 1;
            case 181:
                imageOffsetFromBack += 1;
            case 172:
                imageOffsetFromBack += 2;
            case 150:
                imageOffsetFromBack += 1;
            case 142:
                imageOffsetFromBack += 1;
            case 130:
                imageOffsetFromBack += 1;
            case 127:
                imageOffsetFromBack += 1;
            case 115:
                imageOffsetFromBack += 2;
            case 105:
                imageOffsetFromBack += 1;
            case 103:
                imageOffsetFromBack += 1;
            case 94:
                imageOffsetFromBack += 1;
            case 89:
                imageOffsetFromBack += 1;
            case 88:
                imageOffsetFromBack += 1;
            case 80:
                imageOffsetFromBack += 1;
            case 76:
                imageOffsetFromBack += 1;
            case 75:
                imageOffsetFromBack += 1;
            case 74:
                imageOffsetFromBack += 1;
            case 65:
                imageOffsetFromBack += 1;
            case 53:
                imageOffsetFromBack += 1;
            case 52:
                imageOffsetFromBack += 1;
            case 51:
                imageOffsetFromBack += 1;
            case 50:
                imageOffsetFromBack += 1;
            case 38:
                imageOffsetFromBack += 1;
            case 37:
                imageOffsetFromBack += 1;
            case 28:
                imageOffsetFromBack += 1;
            case 27:
                imageOffsetFromBack += 1;
            case 26:
                imageOffsetFromBack += 13;
            case 25:
                imageOffsetFromBack += 2;
            case 20:
                imageOffsetFromBack += 1;
            case 19:
                imageOffsetFromBack += 1;
            case 18:
                imageOffsetFromBack += 1;
            case 15:
                imageOffsetFromBack += 1;
            case 9:
                imageOffsetFromBack += 2;
            case 6:
                imageOffsetFromBack += 1;
            case 3:
                imageOffsetFromBack += 0;
        }
#pragma GCC diagnostic pop
        int drawIndex = types_spritesheet_beast_idx + imageOffsetFromBack + form;
        if (drawIndex < types_spritesheet_201_1_idx)
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, drawIndex), x, y, &tint, scale, scale);
        }
        else
        {
            Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_pkm, pkm_spritesheet_0_idx), x, y, &tint, scale, scale);
        }
    }
}

void Gui::ball(size_t index, int x, int y)
{
    if (index < 27)
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, index + types_spritesheet_empty_idx), x, y);
    }
    else
    {
        Gui::drawImageAt(C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_empty_idx), x, y);
    }
}

static C2D_Image typeImage(Language lang, u8 type)
{
    if (type > 17)
    {
        type = 0;
    }
    switch (lang)
    {
        case Language::ES:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_es_00_idx + type);
        case Language::DE:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_de_00_idx + type);
        case Language::FR:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_fr_00_idx + type);
        case Language::IT:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_it_00_idx + type);
        case Language::JP:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_jp_00_idx + type);
        case Language::KO:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_ko_00_idx + type);
        case Language::TW:
        case Language::ZH:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_zh_00_idx + type);
        case Language::EN:
        case Language::PT:
        case Language::NL:
        default:
            return C2D_SpriteSheetGetImage(spritesheet_types, types_spritesheet_en_00_idx + type);
    }
}

void Gui::type(Language lang, u8 type, int x, int y)
{
    Gui::drawImageAt(typeImage(lang, type), x, y);
}

void Gui::setScreen(std::unique_ptr<Screen> screen)
{
    screens.push(std::move(screen));
}

int Gui::pointerBob()
{
    static int currentBob = 0;
    static bool up        = true;
    if (up)
    {
        currentBob++;
        if (currentBob >= 12)
        {
            up = false;
        }
    }
    else
    {
        currentBob--;
        if (currentBob <= 0)
        {
            up = true;
        }
    }
    return currentBob / 4;
}

u8 transparencyWaver()
{
    static u8 currentAmount = 255;
    static bool dir         = true;
    if (!dir)
    {
        currentAmount++;
        if (currentAmount == 255)
            dir = true;
    }
    else
    {
        currentAmount--;
        if (currentAmount < 155)
            dir = false;
    }
    return currentAmount;
}

bool Gui::showChoiceMessage(const std::string& message, std::optional<std::string> message2, int timer)
{
    u32 keys = 0;
    if (inFrame)
    {
        C3D_FrameEnd(0);
    }
    hidScanInput();
    while (aptMainLoop() && !((keys = hidKeysDown()) & KEY_B))
    {
        hidScanInput();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        Gui::clearScreen(GFX_TOP);
        Gui::clearScreen(GFX_BOTTOM);

        target(GFX_TOP);
        sprite(ui_sheet_part_info_top_idx, 0, 0);
        if (!message2)
        {
            text(message, 200, 95, FONT_SIZE_15, FONT_SIZE_15, C2D_Color32(255, 255, 255, transparencyWaver()), TextPosX::CENTER, TextPosY::TOP);
        }
        else
        {
            u8 transparency = transparencyWaver();
            text(message, 200, 85, FONT_SIZE_15, FONT_SIZE_15, C2D_Color32(255, 255, 255, transparency), TextPosX::CENTER, TextPosY::TOP);
            text(message2.value(), 200, 105, FONT_SIZE_15, FONT_SIZE_15, C2D_Color32(255, 255, 255, transparency), TextPosX::CENTER, TextPosY::TOP);
        }

        text(i18n::localize("CONTINUE_CANCEL"), 200, 130, FONT_SIZE_11, FONT_SIZE_11, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);

        flushText();

        target(GFX_BOTTOM);
        sprite(ui_sheet_part_info_bottom_idx, 0, 0);

        if (!aptIsHomeAllowed() && aptIsHomePressed())
        {
            setDoHomeDraw();
        }

        drawNoHome();

        C3D_FrameEnd(0);
        if (timer)
        {
            svcSleepThread(timer);
            timer = 0;
        }
        if (keys & KEY_A)
        {
            hidScanInput();
            if (inFrame)
            {
                C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            }
            return true;
        }
    }
    hidScanInput();
    if (inFrame)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
    return false;
}

void Gui::waitFrame(const std::string& message, std::optional<std::string> message2)
{
    if (inFrame)
    {
        C3D_FrameEnd(0);
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    Gui::clearScreen(GFX_TOP);
    Gui::clearScreen(GFX_BOTTOM);

    target(GFX_TOP);
    sprite(ui_sheet_part_info_top_idx, 0, 0);
    if (!message2)
    {
        text(message, 200, 95, FONT_SIZE_15, FONT_SIZE_15, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);
    }
    else
    {
        text(message, 200, 85, FONT_SIZE_15, FONT_SIZE_15, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);
        text(message2.value(), 200, 105, FONT_SIZE_15, FONT_SIZE_15, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);
    }

    text(i18n::localize("PLEASE_WAIT"), 200, 130, FONT_SIZE_11, FONT_SIZE_11, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);

    flushText();

    target(GFX_BOTTOM);
    sprite(ui_sheet_part_info_bottom_idx, 0, 0);

    C3D_FrameEnd(0);

    if (inFrame)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
}

void Gui::warn(const std::string& message, std::optional<std::string> message2, std::optional<Language> lang)
{
    u32 keys = 0;
    if (inFrame)
    {
        C3D_FrameEnd(0);
    }
    hidScanInput();
    while (aptMainLoop() && !((keys = hidKeysDown()) & KEY_A))
    {
        hidScanInput();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        Gui::clearScreen(GFX_TOP);
        Gui::clearScreen(GFX_BOTTOM);

        target(GFX_TOP);
        sprite(ui_sheet_part_info_top_idx, 0, 0);
        if (!message2)
        {
            text(message, 200, 95, FONT_SIZE_15, FONT_SIZE_15, C2D_Color32(255, 255, 255, transparencyWaver()), TextPosX::CENTER, TextPosY::TOP);
        }
        else
        {
            u8 transparency = transparencyWaver();
            text(message, 200, 85, FONT_SIZE_15, FONT_SIZE_15, C2D_Color32(255, 255, 255, transparency), TextPosX::CENTER, TextPosY::TOP);
            text(message2.value(), 200, 105, FONT_SIZE_15, FONT_SIZE_15, C2D_Color32(255, 255, 255, transparency), TextPosX::CENTER, TextPosY::TOP);
        }

        if (lang)
        {
            text(i18n::localize(lang.value(), "CONTINUE"), 200, 130, FONT_SIZE_11, FONT_SIZE_11, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);
        }
        else
        {
            text(i18n::localize("CONTINUE"), 200, 130, FONT_SIZE_11, FONT_SIZE_11, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);
        }

        flushText();

        target(GFX_BOTTOM);

        sprite(ui_sheet_part_info_bottom_idx, 0, 0);

        if (!aptIsHomeAllowed() && aptIsHomePressed())
        {
            setDoHomeDraw();
        }

        drawNoHome();

        C3D_FrameEnd(0);
    }
    hidScanInput();
    if (inFrame)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
}

void Gui::screenBack()
{
    screens.pop();
}

void Gui::showRestoreProgress(u32 partial, u32 total)
{
    if (inFrame)
    {
        C3D_FrameEnd(0);
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    Gui::clearScreen(GFX_TOP);
    Gui::clearScreen(GFX_BOTTOM);
    target(GFX_TOP);
    sprite(ui_sheet_part_info_top_idx, 0, 0);
    text(i18n::localize("SAVING"), 200, 95, FONT_SIZE_15, FONT_SIZE_15, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);
    text(StringUtils::format(i18n::localize("SAVE_PROGRESS"), partial, total), 200, 130, FONT_SIZE_12, FONT_SIZE_12, COLOR_WHITE, TextPosX::CENTER,
        TextPosY::TOP);
    flushText();

    target(GFX_BOTTOM);
    sprite(ui_sheet_part_info_bottom_idx, 0, 0);

    C3D_FrameEnd(0);

    if (inFrame)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
}

void Gui::showDownloadProgress(const std::string& path, u32 partial, u32 total)
{
    if (inFrame)
    {
        C3D_FrameEnd(0);
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    Gui::clearScreen(GFX_TOP);
    Gui::clearScreen(GFX_BOTTOM);
    target(GFX_TOP);
    sprite(ui_sheet_part_info_top_idx, 0, 0);
    text(StringUtils::format(i18n::localize("DOWNLOADING_FILE"), path.c_str()), 200, 95, FONT_SIZE_15, FONT_SIZE_15, COLOR_WHITE, TextPosX::CENTER,
        TextPosY::TOP);
    text(StringUtils::format(i18n::localize("SAVE_PROGRESS"), partial, total), 200, 130, FONT_SIZE_12, FONT_SIZE_12, COLOR_WHITE, TextPosX::CENTER,
        TextPosY::TOP);
    flushText();

    target(GFX_BOTTOM);
    sprite(ui_sheet_part_info_bottom_idx, 0, 0);

    C3D_FrameEnd(0);

    if (inFrame)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
}

void Gui::showResizeStorage()
{
    if (inFrame)
    {
        C3D_FrameEnd(0);
    }

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    Gui::clearScreen(GFX_TOP);
    Gui::clearScreen(GFX_BOTTOM);
    target(GFX_TOP);
    sprite(ui_sheet_part_info_top_idx, 0, 0);
    text(i18n::localize("STORAGE_RESIZE"), 200, 95, FONT_SIZE_15, FONT_SIZE_15, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);
    flushText();

    target(GFX_BOTTOM);
    sprite(ui_sheet_part_info_bottom_idx, 0, 0);

    C3D_FrameEnd(0);

    if (inFrame)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
}

void Gui::error(const std::string& message, Result errorCode)
{
    u32 keys = 0;
    if (inFrame)
    {
        C3D_FrameEnd(0);
    }
    hidScanInput();
    while (aptMainLoop() && !((keys = hidKeysDown()) & KEY_A))
    {
        hidScanInput();
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        Gui::clearScreen(GFX_TOP);
        Gui::clearScreen(GFX_BOTTOM);

        target(GFX_TOP);
        sprite(ui_sheet_part_info_top_idx, 0, 0);
        u8 transparency = transparencyWaver();
        text(message, 200, 85, FONT_SIZE_15, FONT_SIZE_15, C2D_Color32(255, 255, 255, transparency), TextPosX::CENTER, TextPosY::TOP);
        text(StringUtils::format(i18n::localize("ERROR_CODE"), errorCode), 200, 105, FONT_SIZE_15, FONT_SIZE_15,
            C2D_Color32(255, 255, 255, transparency), TextPosX::CENTER, TextPosY::TOP);

        text(i18n::localize("CONTINUE"), 200, 130, FONT_SIZE_11, FONT_SIZE_11, COLOR_WHITE, TextPosX::CENTER, TextPosY::TOP);

        flushText();

        target(GFX_BOTTOM);
        sprite(ui_sheet_part_info_bottom_idx, 0, 0);

        if (!aptIsHomeAllowed() && aptIsHomePressed())
        {
            setDoHomeDraw();
        }

        drawNoHome();

        C3D_FrameEnd(0);
    }
    hidScanInput();
    if (inFrame)
    {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    }
}

void Gui::drawSelector(float x, float y)
{
    static constexpr int w     = 2;
    static float timer         = 0.0f;
    float highlight_multiplier = fmax(0.0, fabs(fmod(timer, 1.0) - 0.5) / 0.5);
    u8 r                       = COLOR_SELECTOR & 0xFF;
    u8 g                       = (COLOR_SELECTOR >> 8) & 0xFF;
    u8 b                       = (COLOR_SELECTOR >> 16) & 0xFF;
    u32 color = C2D_Color32(r + (255 - r) * highlight_multiplier, g + (255 - g) * highlight_multiplier, b + (255 - b) * highlight_multiplier, 255);

    Gui::drawSolidRect(x, y, 50, 50, C2D_Color32(255, 255, 255, 100));
    Gui::drawSolidRect(x, y, 50, w, color);                      // top
    Gui::drawSolidRect(x, y + w, w, 50 - 2 * w, color);          // left
    Gui::drawSolidRect(x + 50 - w, y + w, w, 50 - 2 * w, color); // right
    Gui::drawSolidRect(x, y + 50 - w, 50, w, color);             // bottom

    timer += .025f;
}
