#include "gba.h"
#pragma GCC optimize("O0")

// Hide sprite number N by setting its disable flag
// Preferred if it is important to retain its position
void HideSprite(int N)
{
	OBJ_ATTR(N, 0) |= ATTR0_HIDE;
}

// Show sprite number N by clearing its disable flag
void ShowSprite(int N)
{
	OBJ_ATTR(N, 0) &= ~ATTR0_HIDE;		// attr0: hide
}

// Moves the sprite of index N to the position specified
void MoveSprite(int N, int x, int y)
{
	OBJ_ATTR(N, 0) &= ~0x00FF;					// clear the current y-value
	OBJ_ATTR(N, 0) |= y & 0x00FF;				// apply the new y-value
	OBJ_ATTR(N, 1) &= ~0x01FF;					// same for x-value...
	OBJ_ATTR(N, 1) |= x & 0x01FF;
}

// Move the sprite outside of visual boundary
// Preferred if the sprite is no longer used, then ShowSprite will not work!
void HideSpriteOffscreen(int N)
{
	MoveSprite(N, 240, 160);
}

void SetSpriteBaseIndex(int N, int tid)
{
	OBJ_ATTR(N, 2) &= ~0x03FF;				// clear the current tile index
	OBJ_ATTR(N, 2) |= (tid & 0x03FF);		// apply the new tile index
}

void SetSpritePalbank(int N, int p)
{
	OBJ_ATTR(N, 2) &= ~0xF000;				// clear current palbank
	OBJ_ATTR(N, 2) |= ATTR2_PALBANK(p);		// apply new palbank
}

// Old function that overwrites a lot of settings
// Only use for initializing memory!
void DrawSprite(int id, int N, int x, int y)
{
    // Displays sprite number numb on screen at position (x,y), as sprite object N
    OBJ_ATTR(N, 0) = y | ATTR0_8BPP;
    OBJ_ATTR(N, 1) = x;				// attr1
    OBJ_ATTR(N, 2) = id;			// attr2: base sprite index
}
