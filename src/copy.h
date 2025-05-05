// Copy 8 words (32 bytes) at a time for palette and sprite data transfer
// (Palettes must have a multiple of 16 colors)
extern void BigDataCopy(u32 destination, u32 source, u32 size);
