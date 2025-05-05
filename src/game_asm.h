// Constants to use in DrawFrogSprite (no preprocessor for assembly...)
// #define TID_FROG_F			48
// #define TID_FROG_B			56
// #define TID_FROG_R			64
// #define TID_FROG_AIR_ADD		4
// #define PALBANK_ADD			1
// #define PALBANK_FROZEN		4

// Draw a frog sprite, unhiding it
// int oam_num
// int scr_x
// int scr_y
// int facing		Use direction enum
// int jump			Whether frog is in the air
// int type			Frog color (use defines)
// int frozen
// int priority		1 => in front of fairy, 2 => behind fairy
extern void DrawFrogSprite(int oam_num, int scr_x, int scr_y, int facing, int jump, int type, int frozen, int priority);
