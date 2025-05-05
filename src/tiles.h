// This file must be defined after sprites.h!

// Palette
const u16 BgPalette[] =
{
	0,					// not usable
	Color(0x465),		// water main
	Color(0x687),		// border
	Color(0x000)		// pause menu
};

// Environment tileset (charblock 0)
const u8 Charblock0Data[] =
{
	// Empty (tile 0)
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	
	// Border (tile 1)
	1, 1, 1, 2, 1, 1, 1, 2,
	1, 1, 2, 1, 1, 1, 2, 2,
	1, 2, 1, 1, 1, 2, 1, 2,
	2, 1, 1, 1, 2, 1, 1, 2,
	1, 1, 1, 2, 1, 1, 1, 2,
	1, 1, 2, 1, 1, 1, 2, 2,
	1, 2, 1, 1, 1, 2, 1, 2,
	2, 1, 1, 1, 2, 1, 1, 2,
	
	// Menu overlay (tile 2)
	3, 0, 3, 0, 3, 0, 3, 0,
	0, 3, 0, 3, 0, 3, 0, 3,
	3, 0, 3, 0, 3, 0, 3, 0,
	0, 3, 0, 3, 0, 3, 0, 3,
	3, 0, 3, 0, 3, 0, 3, 0,
	0, 3, 0, 3, 0, 3, 0, 3,
	3, 0, 3, 0, 3, 0, 3, 0,
	0, 3, 0, 3, 0, 3, 0, 3
};

// Generate screenblocks in DataSetup
// The water background will use BG0, charblock 0, screenblock 8
// The pause menu background will use BG0, charblock 0, screenblock 9
