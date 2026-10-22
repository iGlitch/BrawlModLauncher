#include "GraphicsScene.h"
#include "../Network/networkloader.h"
#include "textures.h"
#include "../Common.h"

void GraphicsScene::drawNewsBox(f32 & yPos, f32 offset, f32 height)
{

	if (m_fCurrentNewsScrollFrame < m_fMaxNewsScrollFrames)
		m_fCurrentNewsScrollFrame++;
	else
		m_fCurrentNewsScrollFrame = 0L;

	//Top White Line
	drawBox(0.0f, yPos + offset, m_fScreenWidth, 2, 255, 255, 255, 255);
	yPos += 2.0f;

	//Transparent New Box
	drawBox(0.0f, yPos + offset, m_fScreenWidth, 65, u8(0), u8(0), u8(0), 95);


	yPos += 65.0f / 2.0f;


	if (newsText != NULL)
	{
		ChangeFontSize(24);
		if (!fontSystem[24])
			fontSystem[24] = new FreeTypeGX(24);

		wchar_t * wText = (wchar_t*)malloc(sizeof(wchar_t)* 4096);
		swprintf(wText, 4096, newsText);
		float fNewsTextWidth = fontSystem[24]->getWidth(wText);
		float startOffset = m_fScreenWidth;
		float endOffset = -1 * fNewsTextWidth - (m_fScreenWidth / 2.0f); //extra padding
		f32 newsTextAanimationRatio = (f32)m_fCurrentNewsScrollFrame / (f32)m_fMaxNewsScrollFrames;
		fontSystem[24]->drawText((endOffset - startOffset) * newsTextAanimationRatio + startOffset, yPos + offset, wText, (GXColor){ 0xff, 0xff, 0xff, 0xff }, FTGX_JUSTIFY_LEFT | FTGX_ALIGN_MIDDLE);
		delete wText;
	}

	yPos += 65.0f / 2.0f;

	//Bottom White Line
	drawBox(0.0f, yPos + offset, m_fScreenWidth, 2, 255, 255, 255, 255);
	yPos += 2.0f;

	if (isTexturesLoaded())
		drawTexturedBox(&logoTexture, 25, 12 + offset, 180, 60, 255, 255, 255, 255);

}


void GraphicsScene::drawMenuItem(f32 x, f32 y, f32 itemWidth, f32 textureWidth, f32 textureHeight, int animateOnFrame, int animationFrames, GXTexObj *texture, bool selected)
{
	f32 offset = 0.0f;
	//f32 xPos = 0.0f;
	//f32 width = 0.0f;

	f32 selectionAnimationRatio = (f32)m_iMenuSelectionFrame / (f32)m_iMenuSelectionAnimationFrames;
	selectionAnimationRatio = (1.0f - (1.0f - selectionAnimationRatio) * (1.0f - selectionAnimationRatio));
	f32 slideInAnimationRatio = 0.0f;
	if (m_iDrawFrameNumber > (animateOnFrame + animationFrames))
		slideInAnimationRatio = 1.0f;
	else if (m_iDrawFrameNumber < animateOnFrame)
		slideInAnimationRatio = 0.0f;// item(itemWidth / animationFrames) * (frameNumber - animateOnFrame);
	else
		slideInAnimationRatio = ((f32)m_iDrawFrameNumber - (f32)animateOnFrame) / animationFrames;
	slideInAnimationRatio = (1.0f - (1.0f - slideInAnimationRatio) * (1.0f - slideInAnimationRatio));

	//f32 direction = 1.0f;

	drawMenuItemInternal((itemWidth + 31) *(slideInAnimationRatio - 1.0f), y, itemWidth, u8(0), u8(0), u8(0));
	if (selected)
	{
		drawMenuItemInternal((itemWidth + 31) * (slideInAnimationRatio - 1.0f), y - (5 * selectionAnimationRatio), itemWidth - (10 * selectionAnimationRatio), selectionColor.r, selectionColor.g, selectionColor.b);
		if (isTexturesLoaded())
			drawTexturedBox(texture, (itemWidth - (10 * selectionAnimationRatio) - textureWidth + offset) + (itemWidth - (10 * selectionAnimationRatio) - textureWidth + offset + textureWidth) *(slideInAnimationRatio - 1.0f), y + 10 - (5 * selectionAnimationRatio), textureWidth, textureHeight, u8(0), u8(0), u8(0), 255);
	}
	else if (isTexturesLoaded())
		drawTexturedBox(texture, (itemWidth - textureWidth + offset) + (itemWidth - textureWidth + offset + textureWidth) *(slideInAnimationRatio - 1.0f), y + 10, textureWidth, textureHeight, 255, 255, 255, 255);

}

void GraphicsScene::drawMenuItemInternal(f32 x, f32 y, f32 width, u8 red, u8 green, u8 blue)
{

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);			// Draw A Quad

	GX_Position3f32(x, y, 0.0f);				// Top Left
	GX_Color4u8(red, green, blue, 255);

	GX_Position3f32(x + width + 18, y, 0.0f);	// Top Right
	GX_Color4u8(red, green, blue, 255);

	GX_Position3f32(x + width, y + 48, 0.0f);	// Bottom Right	
	GX_Color4u8(red, green, blue, 255);

	GX_Position3f32(x, y + 48, 0.0f);			// Bottom Left
	GX_Color4u8(red, green, blue, 255);

	GX_End();

	GX_Begin(GX_TRIANGLES, GX_VTXFMT0, 3);				// Draw A Triangle

	GX_Position3f32(x + width + 18, y, 0.0f);		// Top Right
	GX_Color4u8(red, green, blue, 255);

	GX_Position3f32(x + width, y + 48, 0.0f);		// Bottom Right	
	GX_Color4u8(red, green, blue, 255);

	GX_Position3f32(x + width + 31, y + 15, 0.0f);	// Bottom Right	
	GX_Color4u8(red, green, blue, 255);

	GX_End();									// Done Drawing The Quad 

}

void GraphicsScene::drawInfoBox(f32 yPos, f32 height, const wchar_t * text)
{
	//Top White Line
	drawBox(m_fScreenWidth / 16.0f, yPos, m_fScreenWidth / 16.0f * 14.0f, 2, 255, 255, 255, 255);
	//Left Line
	drawBox(m_fScreenWidth / 16.0f, yPos, 2, height + 4, 255, 255, 255, 255);
	//Right Line
	drawBox(m_fScreenWidth / 16.0f * 15.0f - 2.0f, yPos, 2, height + 4, 255, 255, 255, 255);


	//Transparent New Box
	drawBox(m_fScreenWidth / 16.0f + 2, yPos + 2.0f, m_fScreenWidth / 16.0f * 14.0f - 4.0f, height, 0, 0, 0, 95);

	//Top White Line
	drawBox(m_fScreenWidth / 16.0f, yPos + 2.0f + height, m_fScreenWidth / 16.0f * 14.0f, 2, 255, 255, 255, 255);

	ChangeFontSize(15);
	if (!fontSystem[15])
		fontSystem[15] = new FreeTypeGX(15);
	fontSystem[15]->drawText(m_fScreenWidth / 2.0f, yPos + 2.0f + (height / 2.0f), text, (GXColor){ 0xff, 0xff, 0xff, 0xff }, FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE);

}


