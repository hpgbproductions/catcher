#pragma GCC optimize("O0")

#include "gba.h"
#include "copy.h"
#include "sprites.h"
#include "tiles.h"
#include "levels.h"
#include "physics.h"
#include "game_asm.h"

#define PROFILER_ENABLED		0
#define PROFILER_FREQUENCY		TIMER_FREQUENCY_1024

// OAM slot to use
#define OBJ_PLAYER				0
#define OBJ_PLAYER_SHADOW		1
#define	OBJ_PLAYER_ATTACK		2				// when using special or hit by bullet

#define OBJ_PLAYER_BULLET(b)	(12 + b)		// 12~15
#define OBJ_FROG_ATTACK(f)		(16 + f)		// 16~31: warning circles and explosions
#define OBJ_FROG_BULLET(b)		(32 + b)		// 32~47
#define OBJ_FROG(f)				(48 + f)		// 48~63
#define OBJ_FROG_SHADOW(f)		(64 + f)		// 64~79

// Reserve 96~111 for digits (n = 0~15)
// 		[All level] n = 0~1 for seconds remaining
//		[All level] n = 2~3 for current level
//		[All level] n = B~F for profiler
#define OBJ_UI_DIGIT(n)			(96 + n)		// 96~111

#define OBJ_UI_BIGTEXT0			112				// left: ready and paused
#define OBJ_UI_BIGTEXT1			113				// right: ready and paused
#define OBJ_UI_BIGTEXT2			114				// center: go
#define OBJ_UI_LEVEL			115
#define OBJ_UI_CHARGE_OVER		116
#define OBJ_UI_CHARGE_BAR		117
#define OBJ_UI_CHARGE_BG		118
#define OBJ_UI_POWER0			119				// box (power 1)
#define OBJ_UI_POWER1			120				// bar (power 2~5)
#define OBJ_UI_POWER2			121				// bar (power 6~9)

// let the displayed timer have 32 ticks per displayed second
#define SECONDS_CEIL(t)	((t>>5) + (t & 0x1F ? 1 : 0))
#define SECONDS(t)		(t>>5)
#define TICKS(s)		(s<<5)
const int TimeLimitSeconds = 60;

enum // 8 directions clockwise from right
{
	EIGHTWAY_RIGHT,
	EIGHTWAY_RIGHT_DOWN,
	EIGHTWAY_DOWN,
	EIGHTWAY_LEFT_DOWN,
	EIGHTWAY_LEFT,
	EIGHTWAY_LEFT_UP,
	EIGHTWAY_UP,
	EIGHTWAY_RIGHT_UP
};

// Set the boundaries for character positions
const struct Vector3 BoundsMin = {16 << 16, 16 << 16, 0 << 16};
const struct Vector3 BoundsMax = {(SCREEN_WIDTH-16) << 16, (SCREEN_HEIGHT-16) << 16, 100 << 16};
const fix32 GoalX = 32 << 16;

// -----------------------------------------------------------------------------
// BEGIN ice fairy/player

// Ice fairy vector constants
const fix32 speed = 0x00030000;					// 3 pixels per tick
const fix32 diagonalSpeed = 0x00024000;			// 2.25 pixels per tick
const struct Vector3 IceFairyStartPos = {56<<16, 80<<16, 0};
const struct Vector3 IceFairySpriteMinPos = {-16<<16, -24<<16, 0};	// Relative position of sprite from the player position
const struct Vector3 IceFairyShadowSpriteMinPos = {-8<<16, 0, 0};

// Input variables
int InputX;
int InputY;
int InputDiagonal;

int InputGrab;
int InputShoot;
int InputSpecial;

int InputGrabPrevious = 0;
int InputShootPrevious = 0;
int InputSpecialPrevious = 0;

int InputPause;
int InputPausePrevious = 0;

// Ice fairy physics
struct Vector3 IceFairyPosition;
struct Vector3 IceFairyVelocity;				// Position difference per frame
struct Vector3 IceFairyBaseVelocity;			// Before multiply by inputs
struct Vector3 IceFairyFacingDirection;			// Facing as vector x and y components
int IceFairyFacing;								// Facing as enum integer
int FrogCarried = -1;
int IceFairyClosenessToCamera = 0;

// Throwing/grabbing
int ThrowCharge = 0;
const int MaxThrowCharge = 30;
const fix32 ThrowBaseHorizontalSpeed	= 0x00030000;
const fix32 ThrowBaseVerticalSpeed 		= 0x00030000;
const fix32 ThrowChargeHorizontalSpeed	= 0x00001C00;
const fix32 ThrowChargeVerticalSpeed	= 0x00000200;
const int GrabRangeSquared = 24*24;									// 24 pixels
const struct Vector3 GrabbedFrogPositionOffset = {0, 0, 48<<16};	// Position of grabbed frog relative to fairy

// Relating to the freezing power
int IceFairyPower = 3;
const int IceFairyMaxPower = 9;
const int IceFairyShootCost = 1;
const int IceFairySpecialCost = 5;
const int ShootFreezeDuration = 160;		// Fairy bullet makes frog freeze for 5 seconds
const int SpecialFreezeDuration = 160;		// Fairy special makes frog freeze for 5 seconds
const int ThawingStart = 64;				// Freeze time left when the frog starts switching color
const fix32 IceFairyBulletSpeed = 8<<16;	// 8 pixels per tick

int SpecialUsedAnimationTimer;				// If >0, display circles representing heat absorption
const int SpecialUsedAnimationDuration = 32;

// END ice fairy/player
// -----------------------------------------------------------------------------
// BEGIN frog

// Frog performance (change per level)
const fix32 frogBaseSpeed = 0x00000004;
const fix32 frogBaseDiagonalSpeed = 0x00000003;
const fix32 frogVerticalSpeed = 0x00040000;
const int frogStaminaRegen = 1;
int frogSpeedMultiplier;
int frogMaxStamina;
int frogSmallJumpCost;
int frogShootCost;
int frogShootWarningDuration;
int frogBulletSpeedMultiplier;		// multiply to frog/fairy distance to set bullet velocity
int frogExplodeCost;
int frogExplodeWarningDuration;

// Computed frog jump velocities
struct Vector3 frogBaseVelocities[8];
struct Vector3 frogSmallJumpVelocities[8];

// Frog constants
const struct Vector3 FrogGravity = {0, 0, -0x00010000};			// Frog gravity
const struct Vector3 FrogSpriteMinPos = {-8<<16, -8<<16, 0};	// Relative position of sprite from frog center
const struct Vector3 FrogShadowSpriteMinPos = {-8<<16, -4<<16, 0};
const int FairyNearSqrDistance = 40*40;			// 40 pixels
const int FairyFarSqrDistance = 70*70;			// 70 pixels

// How frogs see the map
const struct Vector3 Attractor = {140<<16, 80<<16, 0};		// Frogs try to move towards this place
const int AttractorNearSqrDistance = 30*30;		// Closer frogs can move in any direction
const int AttractorFarSqrDistance = 60*60;		// Further frogs must move towards attractor

// Frogs
struct frog_t
{
	int isEnabled;		// Whether it is loaded in the level
	int isFrozen;
	struct Vector3 position;
	struct Vector3 velocity;
	int type;			// Color of frog
	int facing;
	int stamina;
	int action;
	int actionTimer;	// Tick timer where frog cannot change its action, or thaw timer
};
enum // Frog types
{
	TYPE_GREEN,		// normal
	TYPE_YELLOW,	// shooter
	TYPE_RED		// exploding
};
enum // Frog actions
{
	ACT_REST,
	ACT_AIR,
	ACT_SHOOT,
	ACT_EXPLODE
};

struct frog_t DefaultFrog = {FALSE, FALSE, ZERO_VECTOR_INIT, ZERO_VECTOR_INIT, TYPE_GREEN, LOOK_LEFT, 100, ACT_REST, 0};
struct frog_t Frogs[16];

// END frog
// -----------------------------------------------------------------------------
// BEGIN bullet

// Bullets from yellow frogs and player
struct bullet_t
{
	int isEnabled;
	struct Vector3 position;
	struct Vector3 velocity;
	int isFriendly;
};
struct bullet_t DefaultFairyBullet = {FALSE, ZERO_VECTOR_INIT, ZERO_VECTOR_INIT, TRUE};
struct bullet_t DefaultFrogBullet = {FALSE, ZERO_VECTOR_INIT, ZERO_VECTOR_INIT, FALSE};

// Bullet constants
const int FairyBulletHitSqrDistance = 16*16;	// bullet center - target center distance that counts as a hit
const int FrogBulletHitSqrDistance = 12*12;
const struct Vector3 BulletSpriteMinPos = {-8<<16, -8<<16, 0};	// sprite position relative to bullet game position
const struct Vector3 BulletBoundsMin = {-10<<16, -10<<16, 0};		// Bullets outside the bounds will be disabled
const struct Vector3 BulletBoundsMax = {250<<16, 170<<16, 100<<16};
#define MAX_FAIRY_BULLETS 4
#define MAX_FROG_BULLETS 16

// Bullet data
struct bullet_t FairyBullets[MAX_FAIRY_BULLETS];
struct bullet_t FrogBullets[MAX_FROG_BULLETS];
int NumEnabledFrogBullets;

// END bullet
// -----------------------------------------------------------------------------
// BEGIN frog explosion attacks

struct explosion_t
{
	int remainingTicks;
};
struct explosion_t FrogExplosions[16];
struct explosion_t DefaultExplosion = {-1};

const int ExplosionHitSqrDistance = 40*40;
const int ExplosionCooldownTime = 32;	// Cannot charge the next explosion for 1 second
const int ExplosionLifetime = 32;		// 1 second
const int ExplosionSizes[16] =			// read every 2 ticks (use [remainingTicks >> 1])
{
	0, 16, 32, 48, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 48, 32, 16
};

// END explosion
// -----------------------------------------------------------------------------
// BEGIN status

// Status: general
int CurrentLevel = -1;
int NumFrogs = 0;
int CaughtFrogs = 0;
int Paused = FALSE;				// Whether to run ticks, if level is active

enum	// level states
{
	LEVEL_STARTING,
	LEVEL_ACTIVE,
	LEVEL_TIMEOUT,
	LEVEL_DEFEAT,
	LEVEL_COMPLETE,
	LEVEL_ALL_COMPLETE
};
int LevelState;
int DefeatByBullet;				// Used for fairy exploding effect

// Status: time variables
int frame = 0;
int RemainingTicks = 0;

// For game over conditions, exit this number of frames after failing
// 60 = one second
const int TimeoutExitFrame = 180;
const int DefeatExitFrame = 180;

// For the TM2/TM3 timer
int TimerEnabled = FALSE;

// END status

// -----------------------------------------------------------------------------
// Function prototypes
// -----------------------------------------------------------------------------

void ComputeFrogBallistics(int i);
void ComputeFrogClipping(int i);
void FreezeFrog(int i, int duration);

void MoveObjectSprite(u16 oam_index, struct Vector3 position, struct Vector3 spriteOffset);
void MoveShadowSprite(u16 oam_index, struct Vector3 position, struct Vector3 spriteOffset);
int GetSizeAttrValue(int circleType);
void DrawCircleSprite(int oam_index, struct Vector3 circledPosition, int circleType);
void DrawExplosionSprite(int oam_index, struct Vector3 circledPosition, int circleType, int palbank);
int DrawNumberLeft(unsigned int num, int startObj, int x, int y, int clearDigits);
int DrawNumberRight(unsigned int num, int startObj, int x, int y, int clearDigits);

int GetEightWay(struct Vector3 v);
int AverageEightWay(int a, int b);
int GetClosenessToCamera(struct Vector3 v);
void UpdatePowerMeter();

unsigned int GetRandomNumber();

int TimerRemainingSeconds();
void ShowLevelStartSprites();
void OnTimerEnd();
void StartGameTimer(int seconds);
void PauseGameTimer();
void UnpauseGameTimer();

void LoadLevel(int lv);
int Update();

void DataSetup();
void GameSetup();
void SplashSetup();

// -----------------------------------------------------------------------------
// Miscellaneous frog functions
// -----------------------------------------------------------------------------

void ComputeFrogBallistics(int i)
{
	Frogs[i].position = VectorAdd(Frogs[i].position, Frogs[i].velocity);
	Frogs[i].velocity = VectorAdd(Frogs[i].velocity, FrogGravity);
}

void ComputeFrogClipping(int i)
{
	Frogs[i].position = VectorClamp(Frogs[i].position, BoundsMin, BoundsMax);
}

void FreezeFrog(int i, int duration)
{
	if (Frogs[i].isFrozen)
	{
		// Frozen ==> actionTimer stores remaining freeze time
		// Stack new freeze effect on top of existing freeze
		Frogs[i].actionTimer += duration;
	}
	else
	{
		// Not frozen ==> actionTimer stores an attack timer
		// Apply the freeze effect normally, clearing other uses of actionTimer
		Frogs[i].isFrozen = TRUE;
		Frogs[i].actionTimer = duration;
	}
	
	if (Frogs[i].action == ACT_SHOOT || Frogs[i].action == ACT_EXPLODE)
	{
		// Cancel the shoot and explode actions
		Frogs[i].action = ACT_REST;
	}
}

// -----------------------------------------------------------------------------
// Common game sprite functions
// -----------------------------------------------------------------------------

// Sets the position of a sprite of a character object
// which has an origin different from the top left of the sprite
void MoveObjectSprite(u16 oam_index, struct Vector3 position, struct Vector3 spriteOffset)
{
	int x = (position.x + spriteOffset.x) >> 16;
	int y = (position.y - (position.z>>1) + spriteOffset.y) >> 16;
	MoveSprite(oam_index, x, y);
}

// Similar to the fairy/frog calculation, but the z-position is ignored
void MoveShadowSprite(u16 oam_index, struct Vector3 position, struct Vector3 spriteOffset)
{
	int x = (position.x + spriteOffset.x) >> 16;
	int y = (position.y + spriteOffset.y) >> 16;
	MoveSprite(oam_index, x, y);
}

// Convert CIRCLE_* to size attribute (for square sprites)
int GetSizeAttrValue(int circleType)
{
	switch (circleType)
	{
		case CIRCLE_SIZE_64:
			return 3;
		case CIRCLE_SIZE_48:
			return 3;
		case CIRCLE_SIZE_32:
			return 2;
		case CIRCLE_SIZE_16:
			return 1;
		default:
			// CIRCLE_OFF or invalid circle size enum
			return -1;
	}
}

// Draw white circle using CIRCLE_* enum to choose size
void DrawCircleSprite(int oam_index, struct Vector3 circledPosition, int circleType)
{
	int SizeAttrValue = GetSizeAttrValue(circleType);		// For ATTR1_SIZE(*)
	
	// Check for invalid values
	if (SizeAttrValue < 0)
	{
		HideSpriteOffscreen(oam_index);
		return;
	}
	
	// Setup circle parameters except for position
	OBJ_ATTR(oam_index, 0) = ATTR0_SQUARE | ATTR0_4BPP;
	OBJ_ATTR(oam_index, 1) = ATTR1_SIZE(SizeAttrValue);
	OBJ_ATTR(oam_index, 2) = ATTR2_PALBANK(PALBANK_RED) | ATTR2_PRIO(1) | ATTR2_ID(OBJ_BASEINDEX_CIRCLE(circleType));
	
	int SizeHalf = 4 << SizeAttrValue;			// Half length of sprite
	struct Vector3 CircleSpriteOffset = {-SizeHalf << 16, -SizeHalf << 16, 0};
	
	// Move circle to position
	MoveObjectSprite(oam_index, circledPosition, CircleSpriteOffset);
}

// Draw explosion using CIRCLE_* enum to choose size
void DrawExplosionSprite(int oam_index, struct Vector3 circledPosition, int circleType, int palbank)
{
	int SizeAttrValue = GetSizeAttrValue(circleType);		// For ATTR1_SIZE(*)
	
	// Check for invalid values
	if (SizeAttrValue < 0)
	{
		HideSpriteOffscreen(oam_index);
		return;
	}
	
	// Setup circle parameters except for position
	OBJ_ATTR(oam_index, 0) = ATTR0_SQUARE | ATTR0_4BPP;
	OBJ_ATTR(oam_index, 1) = ATTR1_SIZE(SizeAttrValue);
	OBJ_ATTR(oam_index, 2) = ATTR2_PALBANK(palbank) | ATTR2_PRIO(1) | ATTR2_ID(OBJ_BASEINDEX_EXPLOSION(circleType));
	
	int SizeHalf = 4 << SizeAttrValue;			// Half length of sprite
	struct Vector3 CircleSpriteOffset = {-SizeHalf << 16, -SizeHalf << 16, 0};
	
	// Move circle to position
	MoveObjectSprite(oam_index, circledPosition, CircleSpriteOffset);
}

// Draw left-aligned unsigned integer, taking objects starting from startObj
// Use clearDigits to choose a "clear buffer" size, unused sprites will be hidden
// Returns the number of digits drawn
// CAUTION: no built-in limit to the number of sprites used, must clamp values
int DrawNumberLeft(unsigned int num, int startObj, int x, int y, int clearDigits)
{
	const int spacing = 9;		// interval of x-coordinates
	int numDigits = 0;
	
	// Set attr2, which includes the digits of each sprite
	// Start with ones place in startObj, tens in the next, and so on
	do
	{
		OBJ_ATTR((startObj + numDigits), 2) = ATTR2_PALBANK(PALBANK_UI) | OBJ_BASEINDEX_DIGIT(num % 10);
		numDigits++;
		num /= 10;
	}
	while (num > 0);
	
	x += numDigits * spacing;
	
	// Set attr0 and attr1, letting i=0 be ones place sprite
	for (int i = 0; i < numDigits; i++)
	{
		x -= spacing;
		OBJ_ATTR((startObj + i), 0) = ATTR0_TALL | ATTR0_Y(y);
		OBJ_ATTR((startObj + i), 1) = ATTR1_SIZE0 | ATTR1_X(x);
	}
	for (int i = numDigits; i < clearDigits; i++)
	{
		HideSpriteOffscreen(startObj + i);
	}
	
	return numDigits;
}

// Draw right-aligned unsigned integer, taking objects starting from startObj
// Same usage as above. Rightmost sprite is drawn at (x - spacing, y)
int DrawNumberRight(unsigned int num, int startObj, int x, int y, int clearDigits)
{
	const int spacing = 9;		// interval of x-coordinates
	int numDigits = 0;
	
	// Set attr2, which includes the digits of each sprite
	// Start with ones place in startObj, tens in the next, and so on
	do
	{
		OBJ_ATTR((startObj + numDigits), 2) = ATTR2_PALBANK(PALBANK_UI) | OBJ_BASEINDEX_DIGIT(num % 10);
		numDigits++;
		num /= 10;
	}
	while (num > 0);
	
	// Set attr0 and attr1, letting i=0 be ones place sprite
	for (int i = 0; i < numDigits; i++)
	{
		x -= spacing;
		OBJ_ATTR((startObj + i), 0) = ATTR0_TALL | ATTR0_Y(y);
		OBJ_ATTR((startObj + i), 1) = ATTR1_SIZE0 | ATTR1_X(x);
	}
	for (int i = numDigits; i < clearDigits; i++)
	{
		HideSpriteOffscreen(startObj + i);
	}
	
	return numDigits;
}

// -----------------------------------------------------------------------------
// Miscellaneous game functions
// -----------------------------------------------------------------------------

// Convert x- and y- values to an eight-way direction
int GetEightWay(struct Vector3 v)
{
	fix32 x = v.x;
	fix32 y = v.y;
	
	// Use gradients 1/2 or 2 as approximations of 22.5 degree slices
	// Always work with positive x so shifting works
	
	if (x >= 0)			// -90 <= deg <= 90: right or vertical
	{
		if (y >= 0)		// 0 <= deg <= 90: right and (down or horizontal)
		{
			if (y < (x >> 1))			// y < x/2
				return EIGHTWAY_RIGHT;
			else if (y < (x << 1))		// y < 2x
				return EIGHTWAY_RIGHT_DOWN;
			else
				return EIGHTWAY_DOWN;
		}
		else			// -90 <= deg < 0: right and up, close to horizontal still possible
		{
			if (-y < (x >> 1))			// -y < x/2 ==>		y > -x/2
				return EIGHTWAY_RIGHT;
			else if (-y < (x << 1))		// -y < 2x ==>		y > -2x
				return EIGHTWAY_RIGHT_UP;
			else
				return EIGHTWAY_UP;
		}
	}
	else				// left
	{
		if (y >= 0)		// left and (down or horizontal)
		{
			if (y < ((-x) >> 1))			// y < -x/2
				return EIGHTWAY_LEFT;
			else if (y < ((-x) << 1))		// y < -2x
				return EIGHTWAY_LEFT_DOWN;
			else
				return EIGHTWAY_DOWN;
		}
		else			// left and up, close to horizontal still possible
		{
			if (-y < ((-x) >> 1))			// -y < -x/2 ==>	y > x/2
				return EIGHTWAY_LEFT;
			else if (-y < ((-x) << 1))		// -y < -2x ==>		y > 2x
				return EIGHTWAY_LEFT_UP;
			else
				return EIGHTWAY_UP;
		}
	}
}

// Get the average EightWay direction, taking into account looping before 0 or past 7
int AverageEightWay(int a, int b)
{
	// Let b >= a
	if (b < a)
	{
		b = b + 8;
	}
	
	int ClockwiseDifference = b - a;			// always positive since b >= a
	int dir;									// output
	
	if (ClockwiseDifference < 4)
	{
		// Clockwise rotation is smaller ==> add towards b
		dir = a + (ClockwiseDifference >> 1);
	}
	else
	{
		// Anticlockwise rotation is smaller ==> subtract towards b
		dir = a - (ClockwiseDifference >> 1);
	}
	
	// Ensure normalized into the range 0..7
	return (dir >= 8) ? (dir - 8) : ((dir < 0) ? (dir + 8) : dir);
}

// Generate a layer sorting index to compare fairy and frogs
// (larger is closer, and should be drawn with a lower PRIO)
int GetClosenessToCamera(struct Vector3 v)
{
	return v.y + v.z;
}

void UpdatePowerMeter()
{
	// Big box on top left
	// BOX0: red X symbol ==> no power
	// BOX1: blue * symbol ==> has power
	SetSpriteBaseIndex(OBJ_UI_POWER0, IceFairyPower ? OBJ_BASEINDEX_POWER_BOX1 : OBJ_BASEINDEX_POWER_BOX0);
	
	// Draw the 2~5 and 6~9 bars
	switch (IceFairyPower)
	{
		case 9:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR4);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR4);
			break;
		case 8:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR4);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR3);
			break;
		case 7:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR4);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR2);
			break;
		case 6:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR4);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR1);
			break;
		case 5:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR4);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR0);
			break;
		case 4:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR3);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR0);
			break;
		case 3:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR2);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR0);
			break;
		case 2:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR1);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR0);
			break;
		default:
			SetSpriteBaseIndex(OBJ_UI_POWER1, OBJ_BASEINDEX_POWER_BAR0);
			SetSpriteBaseIndex(OBJ_UI_POWER2, OBJ_BASEINDEX_POWER_BAR0);
			break;
	}
}

// -----------------------------------------------------------------------------
// Pseudo-random (linear congruential generator)
// -----------------------------------------------------------------------------

// L'Ecuyer, Pierre (January 1999).
// "Tables of Linear Congruential Generators of Different Sizes and Good Lattice Structure" (PDF).
// Mathematics of Computation. 68 (225): 249 260.
// https://www.ams.org/journals/mcom/1999-68-225/S0025-5718-99-00996-5/S0025-5718-99-00996-5.pdf
// Table 5, first entry for m = 2^31

// Caution:
// Prefer using the more significant bits as they are more random

const unsigned int lcg_a = 594156893;
// const unsigned int lcg_c = 0;
// const unsigned int lcg_m = 1 << 31;		// 2^31 == 0x80000000, then mod m ==> & 0xFFFFFFFF (do nothing)

unsigned int lcg_prev = 0x99999999;			// previous generated number (must start as a large value)

unsigned int GetRandomNumber()
{
	int x = (lcg_a * lcg_prev);
	lcg_prev = x;							// update for the next run
	return x;
}

// -----------------------------------------------------------------------------
// Game state-change timer and its interrupts
// -----------------------------------------------------------------------------

// Returns the ceiling of remaining time in seconds
// ==> remaining TM3 ticks until it overflows, or 0 if it just overflowed
int TimerRemainingSeconds()
{
	return REG_TM3D == 0 ? 0 : 65536 - REG_TM3D;
}

void ShowLevelStartSprites()
{
	// Show the "ready" sprites
	OBJ_ATTR(OBJ_UI_BIGTEXT0, 0) = ATTR0_WIDE | ATTR0_Y(72);
	OBJ_ATTR(OBJ_UI_BIGTEXT0, 1) = ATTR1_SIZE2 | ATTR1_X(88);
	OBJ_ATTR(OBJ_UI_BIGTEXT0, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO3 | ATTR2_ID(OBJ_BASEINDEX_READY0);
	OBJ_ATTR(OBJ_UI_BIGTEXT1, 0) = ATTR0_WIDE | ATTR0_Y(72);
	OBJ_ATTR(OBJ_UI_BIGTEXT1, 1) = ATTR1_SIZE2 | ATTR1_X(120);
	OBJ_ATTR(OBJ_UI_BIGTEXT1, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO3 | ATTR2_ID(OBJ_BASEINDEX_READY1);
	
	// Hide the "go" sprite in case the previous level was finished quickly
	HideSpriteOffscreen(OBJ_UI_BIGTEXT2);
}

// This is run when TM3 (seconds counter) overflows
void OnTimerEnd()
{
	// Disable timers
	REG_TM2CNT = 0;
	REG_TM3CNT = 0;
	TimerEnabled = FALSE;
	
	// Set the next states and run the start/end countdown animations
	switch (LevelState)
	{
		case LEVEL_STARTING:
			// Show "go" text
			OBJ_ATTR(OBJ_UI_BIGTEXT2, 0) = ATTR0_WIDE | ATTR0_Y(72);
			OBJ_ATTR(OBJ_UI_BIGTEXT2, 1) = ATTR1_SIZE2 | ATTR1_X(104);
			OBJ_ATTR(OBJ_UI_BIGTEXT2, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO3 | ATTR2_ID(OBJ_BASEINDEX_GO);
			
			// Hide "ready" text
			HideSpriteOffscreen(OBJ_UI_BIGTEXT0);
			HideSpriteOffscreen(OBJ_UI_BIGTEXT1);
			
			LevelState = LEVEL_ACTIVE;
			StartGameTimer(3);			// How long to show "go" sprite
			break;
			
		case LEVEL_ACTIVE:
			// Hide the "go" sprite
			HideSpriteOffscreen(OBJ_UI_BIGTEXT2);
			// Note that the tick counter, not hardware timer, is used for game updates
			break;
			
		case LEVEL_COMPLETE:
			// Runs a short time after the last frog is caught
			if (CurrentLevel == MaxLevel)
			{
				// This was the last level, set flag to quit on the next tick
				LevelState = LEVEL_ALL_COMPLETE;
			}
			else
			{
				// Load the next level
				LoadLevel(CurrentLevel + 1);
			}
			break;
	}
}

// Start a timer that sends an interrupt when the timer ends
void StartGameTimer(int seconds)
{
	// Disable timers to allow for proper resetting, if starting before previously ended
	REG_TM2CNT = 0;
	REG_TM3CNT = 0;
	
	// TM2 will overflow every second
	REG_TM2D = 49149;
    REG_TM2CNT = TIMER_ENABLE | TIMER_FREQUENCY_1024;
    
    // TM3 increments when TM2 overflows ==> overflows after a given number of seconds
    REG_TM3D = 65536 - seconds;
    REG_TM3CNT = TIMER_ENABLE | TIMER_INTERRUPTS | TIMER_CASCADE;
    
    TimerEnabled = TRUE;
}

void PauseGameTimer()
{
	REG_TM2CNT |= TIMER_CASCADE;
}

void UnpauseGameTimer()
{
	REG_TM2CNT &= ~TIMER_CASCADE;
}

// -----------------------------------------------------------------------------
// Load a level using its integer index
// -----------------------------------------------------------------------------
void LoadLevel(int lv)
{
	// Iterator
	int i = 0;

	CurrentLevel = lv;
	
	// Apply per-level constants todo
	NumFrogs = LevelNumFrogs[lv];
	// frog level constants here
	
	// Reset level status
	CaughtFrogs = 0;
	frame = 0;
	RemainingTicks = TICKS(TimeLimitSeconds);
	LevelState = LEVEL_STARTING;
	DefeatByBullet = FALSE;
	
	// Reset ice fairy
	IceFairyPosition = IceFairyStartPos;
	IceFairyVelocity = ZERO_VECTOR;
	IceFairyBaseVelocity = ZERO_VECTOR;
	IceFairyFacing = LOOK_RIGHT;
	IceFairyFacingDirection.x = 1;
	IceFairyFacingDirection.y = 0;
	IceFairyFacingDirection.z = 0;
	FrogCarried = -1;
	SpecialUsedAnimationTimer = 0;
	
	// Reset ice fairy sprite
	SetSpriteBaseIndex(OBJ_PLAYER, OBJ_BASEINDEX_PLAYER_R);
	OBJ_ATTR(OBJ_PLAYER, 1) &= ~ATTR1_HFLIP;	// Unflip
	MoveObjectSprite(OBJ_PLAYER, IceFairyPosition, IceFairySpriteMinPos);
	MoveShadowSprite(OBJ_PLAYER_SHADOW, IceFairyPosition, IceFairyShadowSpriteMinPos);
	
	// Reset bullets
	for (i = 0; i < MAX_FAIRY_BULLETS; i++)
	{
		FairyBullets[i].isEnabled = FALSE;
		HideSpriteOffscreen(OBJ_PLAYER_BULLET(i));
	}
	for (i = 0; i < MAX_FROG_BULLETS; i++)
	{
		FrogBullets[i].isEnabled = FALSE;
		HideSpriteOffscreen(OBJ_FROG_BULLET(i));
	}
	NumEnabledFrogBullets = 0;
	
	// Reset explosions and warning circle sprites
	for (i = 0; i < 16; i++)
	{
		FrogExplosions[i].remainingTicks = -1;		// set negative to disable
		HideSpriteOffscreen(OBJ_FROG_ATTACK(i));	// attack sprite is shared by explosion and warning
	}
	
	// Reset ability variables because they are sequential logic
	ThrowCharge = 0;
	InputGrab = FALSE;
	InputGrabPrevious = FALSE;
	InputShootPrevious = FALSE;
	InputSpecialPrevious = FALSE;
	
	// Reset pause button as it is sequential logic
	InputPausePrevious = FALSE;
	
	// Reset game UI elements (except for start animations)
	DrawNumberLeft(TimeLimitSeconds, OBJ_UI_DIGIT(0), 8, 144, 2);	// Ensure start time limit is properly shown
	MoveSprite(OBJ_UI_CHARGE_BAR, 0, 160);							// Throw charge meter in empty position
	
	// Show the current level number and move the "Level" text to match no. of digits
	// Note that the level displayed is 1 more than the level index
	int levelDigits = DrawNumberRight(CurrentLevel+1, OBJ_UI_DIGIT(2), 240, 144, 2);
	MoveSprite(OBJ_UI_LEVEL, 205 - 8*levelDigits, 146);
	
	// Set frog performance
	frogMaxStamina = 100 + CurrentLevel * 10;
	frogSmallJumpCost = max(5, 20 - CurrentLevel);
	frogShootCost = max(10, 40 - CurrentLevel * 2);
	frogExplodeCost = max(25, 80 - CurrentLevel * 4);
	frogShootWarningDuration = min(32, 96 - CurrentLevel * 6);
	frogExplodeWarningDuration = min(64, 128 - CurrentLevel * 6);
	
	frogBulletSpeedMultiplier = 0x0200 + 0x0020 * CurrentLevel;
	
	// Precalculate jump velocities
	// Base speed is 0x0004
	// speed mult of 0x8000 ==> speed is 0x20000 or 2px per tick
	// speed mult of 0x0800*level ==> increase by 1/8 px per level
	frogSpeedMultiplier = 0x8000 + 0x0800 * CurrentLevel;
	for (i = 0; i < 8; i++)
	{
		frogSmallJumpVelocities[i].x = frogBaseVelocities[i].x * frogSpeedMultiplier;
		frogSmallJumpVelocities[i].y = frogBaseVelocities[i].y * frogSpeedMultiplier;
		frogSmallJumpVelocities[i].z = frogBaseVelocities[i].z;
	}
	
	// Load frog settings
	for (i = 0; i < 16; i++)
	{
		if (i < NumFrogs)
		{
			// Create frogs
			Frogs[i].isEnabled = TRUE;
			Frogs[i].isFrozen = FALSE;
			Frogs[i].position.x = FrogStartLocations[lv][i*2] << 16;
			Frogs[i].position.y = FrogStartLocations[lv][i*2 + 1] << 16;
			Frogs[i].position.z = 0;
			Frogs[i].velocity = ZERO_VECTOR;
			Frogs[i].type = FrogTypes[lv][i];
			Frogs[i].facing = LOOK_LEFT;
			Frogs[i].stamina = frogMaxStamina;
			Frogs[i].action = ACT_REST;
			Frogs[i].actionTimer = 0;
			
			// Enable sprites (other settings in DataSetup and Update)
			ShowSprite(OBJ_FROG(i));
			ShowSprite(OBJ_FROG_SHADOW(i));
		}
		else
		{
			// Disable unused frogs
			Frogs[i].isEnabled = FALSE;
			HideSpriteOffscreen(OBJ_FROG(i));
			HideSpriteOffscreen(OBJ_FROG_SHADOW(i));
		}
	}
	
	// For starting animation
	StartGameTimer(2);
	ShowLevelStartSprites();
}
// END LoadLevel

// -----------------------------------------------------------------------------
// Game tick
// -----------------------------------------------------------------------------
int Update()
{
	// Value unused, only call to add frame-by-frame variation
	GetRandomNumber();
	
	// Only update every 2 frames
	frame++;
	if (frame & 0x00000001)
	{
		return 0;
	}
	
	// profiler
	#if (PROFILER_ENABLED)
		REG_TM0CNT = TIMER_ENABLE | PROFILER_FREQUENCY;
	#endif
	
	// Run special functions for ended levels
	switch (LevelState)
	{
		// Cannot return on case LEVEL_STARTING as it will skip the initial draw of frogs
			
		case LEVEL_COMPLETE:
			// Move the ice fairy sprite downwards at 1/4 of normal speed
			IceFairyPosition.y += (speed >> 2);
			
			// Set sprite accordingly
			OBJ_ATTR(OBJ_PLAYER, 1) &= ~ATTR1_HFLIP;		// Unflip
			SetSpriteBaseIndex(OBJ_PLAYER, OBJ_BASEINDEX_PLAYER_F);
			MoveObjectSprite(OBJ_PLAYER, IceFairyPosition, IceFairySpriteMinPos);
			MoveShadowSprite(OBJ_PLAYER_SHADOW, IceFairyPosition, IceFairyShadowSpriteMinPos);
			
			// Exit update
			return 0;
			
		case LEVEL_TIMEOUT:
			// Time up text
			OBJ_ATTR(OBJ_UI_BIGTEXT0, 0) = ATTR0_WIDE | ATTR0_Y(72);
			OBJ_ATTR(OBJ_UI_BIGTEXT0, 1) = ATTR1_SIZE2 | ATTR1_X(88);
			OBJ_ATTR(OBJ_UI_BIGTEXT0, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_TIMEUP0);
			OBJ_ATTR(OBJ_UI_BIGTEXT1, 0) = ATTR0_WIDE | ATTR0_Y(72);
			OBJ_ATTR(OBJ_UI_BIGTEXT1, 1) = ATTR1_SIZE2 | ATTR1_X(120);
			OBJ_ATTR(OBJ_UI_BIGTEXT1, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_TIMEUP1);
			
			// Game over, quit game after some number of ticks
			return frame >= TimeoutExitFrame ? 1 : 0;
		
		case LEVEL_DEFEAT:
			// Defeat text
			OBJ_ATTR(OBJ_UI_BIGTEXT0, 0) = ATTR0_WIDE | ATTR0_Y(72);
			OBJ_ATTR(OBJ_UI_BIGTEXT0, 1) = ATTR1_SIZE2 | ATTR1_X(88);
			OBJ_ATTR(OBJ_UI_BIGTEXT0, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_DEFEAT0);
			OBJ_ATTR(OBJ_UI_BIGTEXT1, 0) = ATTR0_WIDE | ATTR0_Y(72);
			OBJ_ATTR(OBJ_UI_BIGTEXT1, 1) = ATTR1_SIZE2 | ATTR1_X(120);
			OBJ_ATTR(OBJ_UI_BIGTEXT1, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_DEFEAT1);
			
			// Explosion visual effect using player attack oam slot
			if (DefeatByBullet && frame < 16)
			{
				// Set explosion size using array
				const int explosionSizes[16] =
				{
					16, 16, 32, 32, 48, 48, 64, 64,
					64, 48, 48, 32, 32, 16, 16, 0
				};
				DrawExplosionSprite(OBJ_PLAYER_ATTACK, IceFairyPosition, explosionSizes[frame], PALBANK_YELLOW);
			}
			
			// Game over, quit game after some number of ticks
			return frame >= DefeatExitFrame ? 1 : 0;
		
		case LEVEL_ALL_COMPLETE:
			// No more levels, return to start menu immediately
			return 1;
	}
	
	if (LevelState == LEVEL_ACTIVE)
	{
		// BEGIN handle pause menu input
		InputPause = INPUT_ACTIVE(KEY_START);
		if (!InputPausePrevious && InputPause)	// rising edge of InputPause
		{
			// Toggle pause state
			Paused = !Paused;
			
			// Apply pause overlay
			if (Paused)
			{
				REG_DISPCNT |= BG1_ENABLE;
				
				// Show the "paused" sprites
				OBJ_ATTR(OBJ_UI_BIGTEXT0, 0) = ATTR0_WIDE | ATTR0_Y(72);
				OBJ_ATTR(OBJ_UI_BIGTEXT0, 1) = ATTR1_SIZE2 | ATTR1_X(88);
				OBJ_ATTR(OBJ_UI_BIGTEXT0, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_PAUSED0);
				OBJ_ATTR(OBJ_UI_BIGTEXT1, 0) = ATTR0_WIDE | ATTR0_Y(72);
				OBJ_ATTR(OBJ_UI_BIGTEXT1, 1) = ATTR1_SIZE2 | ATTR1_X(120);
				OBJ_ATTR(OBJ_UI_BIGTEXT1, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_PAUSED1);
				
				// Hide the "go" sprite because it looks better
				HideSpriteOffscreen(OBJ_UI_BIGTEXT2);
			}
			else
			{
				REG_DISPCNT &= ~BG1_ENABLE;
				
				// Hide the "paused" sprites
				HideSpriteOffscreen(OBJ_UI_BIGTEXT0);
				HideSpriteOffscreen(OBJ_UI_BIGTEXT1);
			}
		}
		InputPausePrevious = InputPause;
		// END handle pause menu input
		
		// Only continue the update if unpaused
		if (Paused)
		{
			return 0;
		}
		
		// Timer based on the number of ticks
		RemainingTicks--;
		if ((RemainingTicks & 0x1F) == 0)	// RemainingTicks%32==0
		{
			DrawNumberLeft(SECONDS(RemainingTicks), OBJ_UI_DIGIT(0), 8, 144, 2);
		}
		if (RemainingTicks == 0)
		{
			// The next tick and onwards will play the timeout cutscene
			// Unless the level is finished in this tick
			LevelState = LEVEL_TIMEOUT;
			frame = 0;
		}
		
		// Input processing
		InputX = INPUT_ACTIVE(KEY_RIGHT) - INPUT_ACTIVE(KEY_LEFT);
		InputY = INPUT_ACTIVE(KEY_DOWN) - INPUT_ACTIVE(KEY_UP);
		InputDiagonal = InputX && InputY;
		
		InputGrab = INPUT_ACTIVE(KEY_A);
		InputShoot = INPUT_ACTIVE(KEY_L);
		InputSpecial = INPUT_ACTIVE(KEY_R);
	}
	else
	{
		// Suppress movement inputs if level is not active
		InputX = 0;
		InputY = 0;
	}
	
	// BEGIN player movement
	IceFairyBaseVelocity.x = InputX;
	IceFairyBaseVelocity.y = InputY;
	IceFairyVelocity = VectorMultiply(IceFairyBaseVelocity, (InputDiagonal ? diagonalSpeed : speed));
	if (FrogCarried >= 0)
	{
		// Half velocity if carrying a frog
		IceFairyVelocity.x >>= 1;
		IceFairyVelocity.y >>= 1;
	}
	
    IceFairyPosition = VectorAdd(IceFairyPosition, IceFairyVelocity);
    IceFairyPosition = VectorClamp(IceFairyPosition, BoundsMin, BoundsMax);
    
	MoveObjectSprite(OBJ_PLAYER, IceFairyPosition, IceFairySpriteMinPos);
	MoveShadowSprite(OBJ_PLAYER_SHADOW, IceFairyPosition, IceFairyShadowSpriteMinPos);
	IceFairyClosenessToCamera = GetClosenessToCamera(IceFairyPosition);
	// END player movement
	
	// BEGIN set player sprite facing according to input
	if (InputX || InputY)		// Do not change facing if there is no direction input
	{
		if (InputY)
		{
			OBJ_ATTR(OBJ_PLAYER, 1) &= ~ATTR1_HFLIP;		// Unflip
			if (InputY == 1)	// Down pressed
			{
				IceFairyFacing = LOOK_DOWN;
				IceFairyFacingDirection.x = 0;
				IceFairyFacingDirection.y = 1;
				SetSpriteBaseIndex(OBJ_PLAYER, OBJ_BASEINDEX_PLAYER_F);
			}
			else				// Up pressed
			{
				IceFairyFacing = LOOK_UP;
				IceFairyFacingDirection.x = 0;
				IceFairyFacingDirection.y = -1;
				SetSpriteBaseIndex(OBJ_PLAYER, OBJ_BASEINDEX_PLAYER_B);
			}
		}
		else
		{
			SetSpriteBaseIndex(OBJ_PLAYER, OBJ_BASEINDEX_PLAYER_R);
			if (InputX == 1)	// Right pressed
			{
				IceFairyFacing = LOOK_RIGHT;
				IceFairyFacingDirection.x = 1;
				IceFairyFacingDirection.y = 0;
				OBJ_ATTR(OBJ_PLAYER, 1) &= ~ATTR1_HFLIP;	// Unflip
			}
			else				// Left pressed
			{
				IceFairyFacing = LOOK_LEFT;
				IceFairyFacingDirection.x = -1;
				IceFairyFacingDirection.y = 0;
				OBJ_ATTR(OBJ_PLAYER, 1) |= ATTR1_HFLIP;		// Flip
			}
		}
	}
	// END set player sprite facing according to input
	
	// BEGIN Grab/Throw
	if (FrogCarried < 0)	// No frog carried
	{
		if (InputGrabPrevious && !InputGrab)	// falling edge of InputGrab
		{
			int NewFrogCarried = -1;
			int ClosestFrogSqrDistance = GrabRangeSquared;
			
			// Choose the closest frog available, that is closer than defined by GrabRangeSquared
			for (int i = 0; i < NumFrogs; i++)
			{
				int sqrDistance = ApproxSqrDistance2D(IceFairyPosition, Frogs[i].position);
				if (sqrDistance < ClosestFrogSqrDistance)
				{
					// Closer frog in range
					NewFrogCarried = i;
					ClosestFrogSqrDistance = sqrDistance;
				}
			}
			
			// If such a frog is available to be carried
			if (NewFrogCarried >= 0)
			{
				// Actually grab the chosen frog
				FrogCarried = NewFrogCarried;
			}
		}
	}
	else // Carrying a frog
	{
		if (InputGrabPrevious && !InputGrab)	// falling edge of InputGrab
		{
			struct Vector3 throwVelocity =
			{
				IceFairyFacingDirection.x * (ThrowBaseHorizontalSpeed + ThrowCharge*ThrowChargeHorizontalSpeed),
				IceFairyFacingDirection.y * (ThrowBaseHorizontalSpeed + ThrowCharge*ThrowChargeHorizontalSpeed),
				ThrowBaseVerticalSpeed + ThrowCharge*ThrowChargeVerticalSpeed
			};
			
			// Apply to frog
			Frogs[FrogCarried].velocity = throwVelocity;
			Frogs[FrogCarried].action = ACT_AIR;
			FrogCarried = -1;
		}
		
		if (InputGrab)
		{
			// Charge up throwing strength as long as InputGrab is held
			ThrowCharge++;
			ThrowCharge = min(ThrowCharge, MaxThrowCharge);		// Apply maximum strength limit
		}
		else
		{
			// Reset throwing strength when InputGrab is released
			ThrowCharge = 0;
		}
		
		// Release the frog if it is despawned by crossing the goal line
		if (!Frogs[FrogCarried].isEnabled)
		{
			FrogCarried = -1;
			ThrowCharge = 0;
		}
	}
	// Set the charge bar indicator
	MoveSprite(OBJ_UI_CHARGE_BAR, 0, 158 - ThrowCharge);
	// END Grab/Throw
	
	IceFairyPower = clamp(IceFairyPower, 0, 9);		// Constrain power to legal values
	UpdatePowerMeter();
	
	// Player special attack
	// Only try to do on rising edge of InputSpecial and fairy has power
	if (!InputSpecialPrevious && InputSpecial && IceFairyPower >= IceFairySpecialCost)
	{
		IceFairyPower -= IceFairySpecialCost;		// Deduct power
		SpecialUsedAnimationTimer = SpecialUsedAnimationDuration;
		for (int f = 0; f < NumFrogs; f++)			// Freeze all frogs
		{
			FreezeFrog(f, SpecialFreezeDuration);
		}
	}
	
	// Draw the circles visual effect when the special is used
	SpecialUsedAnimationTimer--;
	if (SpecialUsedAnimationTimer > 0)
	{
		// Charging visual effect
		int ringStage = (SpecialUsedAnimationTimer >> 1) & 0x3;
		int diameter = (ringStage + 1) * 16;
		DrawCircleSprite(OBJ_PLAYER_ATTACK, IceFairyPosition, diameter);
	}
	else
	{
		// Hide circle
		HideSpriteOffscreen(OBJ_PLAYER_ATTACK);
	}
	
	// BEGIN player normal attack and bullets
	int TrySpawnBullet = FALSE;
	
	// Only try to do the normal attack on rising edge of InputShoot and fairy has power
	if (!InputShootPrevious && InputShoot && IceFairyPower >= IceFairyShootCost)
	{
		if (FrogCarried >= 0)			// Frog carried, freeze the held frog
		{
			IceFairyPower -= IceFairyShootCost;
			FreezeFrog(FrogCarried, ShootFreezeDuration);
		}
		else							// No frog carried, try to shoot
		{
			TrySpawnBullet = TRUE;
		}
	}
	
	for (int i = 0; i < MAX_FAIRY_BULLETS; i++)
	{
		if (FairyBullets[i].isEnabled)
		{
			// BEGIN update enabled fairy bullet
			
			// Check if out of bounds
			if (!VectorInBounds(FairyBullets[i].position, BulletBoundsMin, BulletBoundsMax))
			{
				// Despawn out-of-bounds bullet
				FairyBullets[i].isEnabled = FALSE;
				HideSpriteOffscreen(OBJ_PLAYER_BULLET(i));
				
				// No further updates for this bullet
				continue;
			}
			
			// Update position
			FairyBullets[i].position = VectorAdd(FairyBullets[i].position, FairyBullets[i].velocity);
			MoveObjectSprite(OBJ_PLAYER_BULLET(i), FairyBullets[i].position, BulletSpriteMinPos);
			
			// Check if it hit a frog
			for (int f = 0; f < NumFrogs; f++)
			{
				if (ApproxSqrDistance(FairyBullets[i].position, Frogs[f].position) < FairyBulletHitSqrDistance)
				{
					FreezeFrog(f, ShootFreezeDuration);		// Freeze the frog that was hit
					FairyBullets[i].isEnabled = FALSE;		// Despawn bullet
					HideSpriteOffscreen(OBJ_PLAYER_BULLET(i));
					break;									// Exit frog check loop
				}
			}
			
			// END update enabled fairy bullet
		}
		else		// bullet not enabled yet
		{
			if (TrySpawnBullet)
			{
				IceFairyPower -= IceFairyShootCost;			// Only deduct if spawning successful
				TrySpawnBullet = FALSE;						// Acknowledge the spawning
				
				FairyBullets[i].isEnabled = TRUE;
				FairyBullets[i].position = IceFairyPosition;
				FairyBullets[i].velocity.x = IceFairyFacing == LOOK_RIGHT ? IceFairyBulletSpeed : (IceFairyFacing == LOOK_LEFT ? -IceFairyBulletSpeed : 0);
				FairyBullets[i].velocity.y = IceFairyFacing == LOOK_DOWN ? IceFairyBulletSpeed : (IceFairyFacing == LOOK_UP ? -IceFairyBulletSpeed : 0);
				FairyBullets[i].velocity.z = 0;
			}
			
			// continue;
		}
	}
	// END player normal attack and bullets
	
	// BEGIN frog action loop
	for (int i = 0; i < NumFrogs; i++)
	{
		// Skip further frog actions if the frog object is disabled (e.g., already caught)
		if (!Frogs[i].isEnabled)
		{
			continue;
		}
		
		// Check if the frog is in the goal
		if (Frogs[i].position.x < GoalX)
		{
			// Update status
			CaughtFrogs++;
			Frogs[i].isEnabled = FALSE;
			IceFairyPower++;				// Reward power
			
			// Release all frog-specific sprites
			HideSpriteOffscreen(OBJ_FROG(i));
			HideSpriteOffscreen(OBJ_FROG_SHADOW(i));
			DrawCircleSprite(OBJ_FROG_ATTACK(i), Frogs[i].position, 0);
			continue;
		}
		
		// Frog examines its surroundings
		struct Vector3 v_FairyToFrog = VectorSubtract(Frogs[i].position, IceFairyPosition);
		int EightWay_FairyToFrog = GetEightWay(v_FairyToFrog);
		int FairySqrDistance = ApproxSqrMagnitude2D(v_FairyToFrog);
		
		struct Vector3 v_FrogToAttractor = VectorSubtract(Attractor, Frogs[i].position);
		int EightWay_FrogToAttractor = GetEightWay(v_FrogToAttractor);
		int AttractorSqrDistance = ApproxSqrMagnitude2D(v_FrogToAttractor);
		
		int JumpDirection;
		int random = GetRandomNumber();
		
		int isCarried = (i == FrogCarried);
			
		// BEGIN frog finite state machine
		if (LevelState == LEVEL_ACTIVE)
		{
			Frogs[i].actionTimer--;
			
			// When the frog is frozen, the action timer represents remaining frozen time
			if (Frogs[i].isFrozen && Frogs[i].actionTimer <= 0)
			{
				Frogs[i].isFrozen = FALSE;
			}
			
			switch (Frogs[i].action)
			{
				case ACT_REST:
					Frogs[i].velocity = ZERO_VECTOR;								// Ensure zero velocity on landing
					Frogs[i].stamina += Frogs[i].isFrozen ? 0: frogStaminaRegen;	// Get stamina if not frozen
					
					// Hide circle in case the frog attack was cancelled
					DrawCircleSprite(OBJ_FROG_ATTACK(i), Frogs[i].position, 0);
					
					// BEGIN branch to choose next action
					if (Frogs[i].isFrozen || Frogs[i].stamina <= 0)
					{
						// Frog cannot perform any action
						// break;
					}
					else if (Frogs[i].type == TYPE_RED && (FairySqrDistance < FairyNearSqrDistance || isCarried))
					{
						// Start charging an explosion
						Frogs[i].action = ACT_EXPLODE;
						Frogs[i].actionTimer = frogExplodeWarningDuration;
					}
					else if (isCarried)
					{
						// Following actions have lower priority than being carried
						// break;
					}
					else if (Frogs[i].type == TYPE_YELLOW && FairySqrDistance > FairyFarSqrDistance && Frogs[i].stamina > frogShootCost)
					{
						// Start charging a shot
						Frogs[i].action = ACT_SHOOT;
						Frogs[i].actionTimer = frogShootWarningDuration;
					}
					else if (FairySqrDistance < FairyFarSqrDistance)
					{
						// Standard action of jumping away
						
						// Choose a jump direction
						if (AttractorSqrDistance < AttractorNearSqrDistance || FairySqrDistance < FairyNearSqrDistance)
						{
							JumpDirection = EightWay_FairyToFrog;
						}
						else if (AttractorSqrDistance < AttractorFarSqrDistance)
						{
							JumpDirection = AverageEightWay(EightWay_FairyToFrog, EightWay_FrogToAttractor);
						}
						else
						{
							JumpDirection = EightWay_FrogToAttractor;
						}
						
						// Random direction adjustment
						JumpDirection += (random & (1<<30)) >> 30;			// +1 if RNG bit 30 is set
						JumpDirection -= (random & (1<<29)) >> 29;			// -1 if RNG bit 29 is set
						
						// Constrain in the range 0..7
						JumpDirection = clamp(JumpDirection, 0, 7);
						
						// jump
						Frogs[i].velocity = frogSmallJumpVelocities[JumpDirection];
						Frogs[i].action = ACT_AIR;
						Frogs[i].stamina -= frogSmallJumpCost;
						ComputeFrogBallistics(i);
						ComputeFrogClipping(i);
						
						// set facing
						if (JumpDirection <= 1 || JumpDirection == 7)
							Frogs[i].facing = LOOK_RIGHT;
						else if (JumpDirection == 2)
							Frogs[i].facing = LOOK_DOWN;
						else if (JumpDirection <= 5)
							Frogs[i].facing = LOOK_LEFT;
						else
							Frogs[i].facing = LOOK_UP;
					}
					// END branch to choose next action
					break;
					// END ACT_REST
					
				case ACT_AIR:
					if (isCarried)
					{
						Frogs[i].action = ACT_REST;		// Cancel flight state
						break;
					}
					
					ComputeFrogBallistics(i);
					ComputeFrogClipping(i);
					if (Frogs[i].position.z == 0)
					{
						Frogs[i].action = ACT_REST;
					}
					break;
					// END ACT_AIR
					
				case ACT_SHOOT:
					// Cancel shooting under these conditions
					if (Frogs[i].isFrozen || isCarried || FairySqrDistance < FairyNearSqrDistance)
					{
						Frogs[i].action = ACT_REST;
						DrawCircleSprite(OBJ_FROG_ATTACK(i), Frogs[i].position, 0);	// Hide circle
						// break;
					}
					else if (Frogs[i].actionTimer > 0)
					{
						// Charging visual effect
						int ringStage = (Frogs[i].actionTimer >> 1) & 0x3;
						int diameter = (ringStage + 1) * 16;
						DrawCircleSprite(OBJ_FROG_ATTACK(i), Frogs[i].position, diameter);
						// break;
					}
					else	// Try to shoot
					{
						// Try to reserve a bullet slot
						int NewBulletIndex = -1;
						for (int bullet = 0; bullet < MAX_FROG_BULLETS; bullet++)
						{
							if (!FrogBullets[bullet].isEnabled)
							{
								NewBulletIndex = bullet;
								break;	// exit for loop
							}
						}
						
						// Don't shoot if there is no bullet slot
						if (NewBulletIndex < 0)
						{
							break;	// exit switch
						}
						// If there is a free bullet slot, execute the following code
						
						// Set new bullet parameters
						struct Vector3 v_FrogToFairy = VectorSubtract(IceFairyPosition, Frogs[i].position);
						struct Vector3 bulletVelocity = VectorMultiplyByFix32(v_FrogToFairy, frogBulletSpeedMultiplier);
						
						FrogBullets[NewBulletIndex].isEnabled = TRUE;
						FrogBullets[NewBulletIndex].position = Frogs[i].position;
						FrogBullets[NewBulletIndex].velocity = bulletVelocity;
						
						DrawCircleSprite(OBJ_FROG_ATTACK(i), Frogs[i].position, 0);	// Hide circle
						
						Frogs[i].stamina -= frogShootCost;		// Deduct stamina
						Frogs[i].action = ACT_REST;				// Return to rest/branch point
					}
					break;
					// END ACT_SHOOT
					
				case ACT_EXPLODE:
					// Cancel explosion under these conditions
					if (Frogs[i].isFrozen)
					{
						Frogs[i].action = ACT_REST;
						DrawCircleSprite(OBJ_FROG_ATTACK(i), Frogs[i].position, 0);	// Hide circle
						// break;
					}
					else if (Frogs[i].actionTimer > 0)
					{
						// Charging visual effect
						int ringStage = (Frogs[i].actionTimer >> 1) & 0x3;
						int diameter = (ringStage + 1) * 16;
						DrawCircleSprite(OBJ_FROG_ATTACK(i), Frogs[i].position, diameter);
						// break;
					}
					else // actionTimer <= 0
					{
						if (FrogExplosions[i].remainingTicks > -ExplosionCooldownTime)			// Greater than
						{
							// Cooldown not satisfied
							// Inhibit actions, keep frog in this state
							// so it cannot move during explosion
						}
						else if (FrogExplosions[i].remainingTicks == -ExplosionCooldownTime)	// Equals
						{
							// Cooldown just completed
							// Allow frog to change actions now
							Frogs[i].action = ACT_REST;				// Return to rest/branch point
						}
						else	// Less than
						{
							// Since the explosion timer has passed the cooldown point,
							// it means that the explosion was from the previous ACT_EXPLODE event
							
							// Allow explosion
							FrogExplosions[i].remainingTicks = ExplosionLifetime;	// Register explosion in the frog's slot
							Frogs[i].stamina -= frogExplodeCost;					// Deduct stamina
						}
					}
					break;
					// END ACT_EXPLODE
			}
		}
		// END frog finite state machine
		
		// Update the explosion that is paired to that red frog
		if (Frogs[i].type == TYPE_RED)
		{			
			if (FrogExplosions[i].remainingTicks >= 0)
			{
				// Decide size, and draw
				int explosionSize = ExplosionSizes[FrogExplosions[i].remainingTicks >> 1];
				DrawExplosionSprite(OBJ_FROG_ATTACK(i), Frogs[i].position, explosionSize, PALBANK_RED);
				
				// Check if fairy is in the kill zone
				if (explosionSize == 64 && FairySqrDistance < ExplosionHitSqrDistance)
				{
					// Player was hit
					LevelState = LEVEL_DEFEAT;					// Set defeated condition
					DefeatByBullet = FALSE;
					frame = 0;									// Reset frame count, which is used to choose when to quit
					SetSpritePalbank(OBJ_PLAYER, PALBANK_BLACK);
				}
			}
			
			// Decrement timer for the next frame
			FrogExplosions[i].remainingTicks--;
		}
		
		// Carried frog will inherit player position and facing
		if (isCarried)
		{
			Frogs[i].position = VectorAdd(IceFairyPosition, GrabbedFrogPositionOffset);	// Fairy's position + some height
			Frogs[i].velocity = ZERO_VECTOR;
			Frogs[i].facing = IceFairyFacing;
		}
		
		DrawFrogSprite
		(
			OBJ_FROG(i),
			(Frogs[i].position.x >> 16) + (FrogSpriteMinPos.x >> 16),
			(Frogs[i].position.y >> 16) - (Frogs[i].position.z >> 17) + (FrogSpriteMinPos.y >> 16),		// (to int) (y - z/2)
			Frogs[i].facing,
			Frogs[i].action == ACT_AIR,
			Frogs[i].type,
			Frogs[i].isFrozen && (Frogs[i].actionTimer > ThawingStart || Frogs[i].actionTimer & 0x08),	// blinking effect if actionTimer is small
			GetClosenessToCamera(Frogs[i].position) > IceFairyClosenessToCamera ? 1 : 2
		);
		MoveShadowSprite(OBJ_FROG_SHADOW(i), Frogs[i].position, FrogShadowSpriteMinPos);
	}
	// END frog action loop
	
	// BEGIN update enabled frog bullets
	for (int i = 0; i < MAX_FROG_BULLETS; i++)
	{
		if (FrogBullets[i].isEnabled)
		{
			// Check if out of bounds
			if (!VectorInBounds(FrogBullets[i].position, BulletBoundsMin, BulletBoundsMax))
			{
				// Despawn out-of-bounds bullet
				FrogBullets[i].isEnabled = FALSE;
				HideSpriteOffscreen(OBJ_FROG_BULLET(i));
				
				// No further updates for this bullet
				continue;
			}
			
			// Update position
			FrogBullets[i].position = VectorAdd(FrogBullets[i].position, FrogBullets[i].velocity);
			MoveObjectSprite(OBJ_FROG_BULLET(i), FrogBullets[i].position, BulletSpriteMinPos);
			
			// Check if it hit the player
			if (ApproxSqrDistance(FrogBullets[i].position, IceFairyPosition) < FrogBulletHitSqrDistance)
			{
				LevelState = LEVEL_DEFEAT;					// Set defeated condition
				DefeatByBullet = TRUE;						// Set flag to show fairy exploding when hit
				frame = 0;									// Reset frame count, which is used to choose when to quit
				SetSpritePalbank(OBJ_PLAYER, PALBANK_BLACK);
				break;										// Exit frog check loop
			}
		}
	}
	// END update enabled frog bullets
		
	// Check if the level is completed
	if (CaughtFrogs >= NumFrogs)
	{
		LevelState = LEVEL_COMPLETE;
		
		IceFairyPower++;								// Reward power
		IceFairyPower = clamp(IceFairyPower, 0, 9);		// Constrain power to legal values
		UpdatePowerMeter();
		
		StartGameTimer(2);				// Start the timer that loads the next level after 2 seconds
	}
	
	// Update "previous" inputs (except pause, which is handled separately)
	InputGrabPrevious = InputGrab;
	InputShootPrevious = InputShoot;
	InputSpecialPrevious = InputSpecial;
	
	// profiler
	#if (PROFILER_ENABLED)
		int timeTaken = REG_TM0D;
		DrawNumberLeft(timeTaken, OBJ_UI_DIGIT(11), 0, 16, 5);
		REG_TM0CNT = 0;
	#endif
	
	return 0;
}
// END Update

// -----------------------------------------------------------------------------
// Preparation
// -----------------------------------------------------------------------------

// Run when booting up
// Fills data arrays
void DataSetup()
{
	// Loop iterators
	int i;
	
	// Sprites
	BigDataCopy((u32)spritePal, (u32)SpritePalette, sizeof(SpritePalette));
    BigDataCopy((u32)spriteData, (u32)Sprites, sizeof(Sprites));
	
	// Background
    BigDataCopy((u32)CHARBLOCK(0), (u32)Charblock0Data, sizeof(Charblock0Data));
    bgPal[1] = BgPalette[1];
    bgPal[2] = BgPalette[2];
    for (i = 0; i < 0x400; i++)
    {
    	// Fill screenblock 8 (map)
    	if ((i & 0x1F) == 3)	// i % 32 == 3
    	{
    		SCREENBLOCK(8)[i] = 0x0001;
    	}
    	else
    	{
    		SCREENBLOCK(8)[i] = 0x0000;
    	}
    	
    	// Fill screenblock 9 (pause menu)
    	SCREENBLOCK(9)[i] = 0x0002;
    }
}

// Run when starting the game
// Writes object data used in levels
void GameSetup()
{
	// Loop iterators
	int i;	
    
    // Hide all sprites
    for (i = 0; i < 128; i++)
    {
        HideSpriteOffscreen(i);
    }
    
    // Frogs
    for (i = 0; i < 16; i++)
	{
		// Reset frog array
		Frogs[i] = DefaultFrog;
		
		// Setup frog sprite (16x16)
		OBJ_ATTR(OBJ_FROG(i), 0) = ATTR0_SQUARE | ATTR0_4BPP | ATTR0_HIDE | ATTR0_Y(160);
    	OBJ_ATTR(OBJ_FROG(i), 1) = ATTR1_SIZE1 | ATTR1_X(240);
    	OBJ_ATTR(OBJ_FROG(i), 2) = ATTR2_ID(OBJ_BASEINDEX_FROG_R_GND) | ATTR2_PRIO(2) | ATTR2_PALBANK(PALBANK_GREEN);
    	
    	// Setup shadow sprite (16x16)
    	OBJ_ATTR(OBJ_FROG_SHADOW(i), 0) = ATTR0_SQUARE | ATTR0_4BPP | ATTR0_HIDE | ATTR0_Y(160);
    	OBJ_ATTR(OBJ_FROG_SHADOW(i), 1) = ATTR1_SIZE1 | ATTR1_X(240);
    	OBJ_ATTR(OBJ_FROG_SHADOW(i), 2) = ATTR2_ID(OBJ_BASEINDEX_SHADOW16) | ATTR2_PRIO(3) | ATTR2_PALBANK(PALBANK_PLAYER);
	}
    
    // Setup player sprite (32x32)
    OBJ_ATTR(OBJ_PLAYER, 0) = ATTR0_SQUARE | ATTR0_4BPP;
    OBJ_ATTR(OBJ_PLAYER, 1) = ATTR1_SIZE2;
    OBJ_ATTR(OBJ_PLAYER, 2) = ATTR2_ID(OBJ_BASEINDEX_PLAYER_R) | ATTR2_PRIO(2) | ATTR2_PALBANK(PALBANK_PLAYER);
    
    // Setup player shadow sprite (16x16)
    OBJ_ATTR(OBJ_PLAYER_SHADOW, 0) = ATTR0_SQUARE | ATTR0_4BPP;
    OBJ_ATTR(OBJ_PLAYER_SHADOW, 1) = ATTR1_SIZE1;
    OBJ_ATTR(OBJ_PLAYER_SHADOW, 2) = ATTR2_ID(OBJ_BASEINDEX_SHADOW16) | ATTR2_PRIO(3) | ATTR2_PALBANK(PALBANK_PLAYER);
    
    // Player attack/exploded sprite will be set on use as it is varied
    
    // Setup charge throw indicator (never changes except for bar position)
	OBJ_ATTR(OBJ_UI_CHARGE_BG, 0) = ATTR0_TALL | ATTR0_4BPP | ATTR0_Y(128);
    OBJ_ATTR(OBJ_UI_CHARGE_BG, 1) = ATTR1_SIZE1 | ATTR1_X(0);
    OBJ_ATTR(OBJ_UI_CHARGE_BG, 2) = ATTR2_ID(OBJ_BASEINDEX_CHARGE_BG) | ATTR2_PRIO(0) | ATTR2_PALBANK(PALBANK_UI);
    OBJ_ATTR(OBJ_UI_CHARGE_BAR, 0) = ATTR0_TALL | ATTR0_4BPP | ATTR0_Y(160);
    OBJ_ATTR(OBJ_UI_CHARGE_BAR, 1) = ATTR1_SIZE1 | ATTR1_X(0);
    OBJ_ATTR(OBJ_UI_CHARGE_BAR, 2) = ATTR2_ID(OBJ_BASEINDEX_CHARGE_BAR) | ATTR2_PRIO(0) | ATTR2_PALBANK(PALBANK_UI);
    OBJ_ATTR(OBJ_UI_CHARGE_OVER, 0) = ATTR0_TALL | ATTR0_4BPP | ATTR0_Y(128);
    OBJ_ATTR(OBJ_UI_CHARGE_OVER, 1) = ATTR1_SIZE1 | ATTR1_X(0);
    OBJ_ATTR(OBJ_UI_CHARGE_OVER, 2) = ATTR2_ID(OBJ_BASEINDEX_CHARGE_OVER) | ATTR2_PRIO(0) | ATTR2_PALBANK(PALBANK_UI);
    
    // Setup power indicator
    IceFairyPower = 3;				// Starting amount of power
    OBJ_ATTR(OBJ_UI_POWER0, 0) = ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(0);
    OBJ_ATTR(OBJ_UI_POWER0, 1) = ATTR1_SIZE1 | ATTR1_X(0);
    OBJ_ATTR(OBJ_UI_POWER0, 2) = ATTR2_ID(OBJ_BASEINDEX_POWER_BOX1) | ATTR2_PRIO(0) | ATTR2_PALBANK(PALBANK_PLAYER);
    OBJ_ATTR(OBJ_UI_POWER1, 0) = ATTR0_WIDE | ATTR0_4BPP | ATTR0_Y(0);
    OBJ_ATTR(OBJ_UI_POWER1, 1) = ATTR1_SIZE0 | ATTR1_X(13);
    OBJ_ATTR(OBJ_UI_POWER1, 2) = ATTR2_ID(OBJ_BASEINDEX_POWER_BAR2) | ATTR2_PRIO(0) | ATTR2_PALBANK(PALBANK_PLAYER);
    OBJ_ATTR(OBJ_UI_POWER2, 0) = ATTR0_WIDE | ATTR0_4BPP | ATTR0_Y(0);
    OBJ_ATTR(OBJ_UI_POWER2, 1) = ATTR1_SIZE0 | ATTR1_X(29);
    OBJ_ATTR(OBJ_UI_POWER2, 2) = ATTR2_ID(OBJ_BASEINDEX_POWER_BAR0) | ATTR2_PRIO(0) | ATTR2_PALBANK(PALBANK_PLAYER);
    
    // Setup "Level" text (never changes except for position)
	OBJ_ATTR(OBJ_UI_LEVEL, 0) = ATTR0_WIDE | ATTR0_Y(146);
	OBJ_ATTR(OBJ_UI_LEVEL, 1) = ATTR1_SIZE2 | ATTR1_X(197);
	OBJ_ATTR(OBJ_UI_LEVEL, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_LEVEL);
    
    // Initialize the frog jump velocity arrays with the defaule
    // Directions are 45 degrees clockwise starting from right
    for (i = 0; i < 8; i++)
    {
    	frogSmallJumpVelocities[i] = ZERO_VECTOR;
    }
    
    // Initialize frog base velocities
    struct Vector3 frogBaseVelocity0 = {	frogBaseSpeed,			0,						frogVerticalSpeed};
	struct Vector3 frogBaseVelocity1 = {	frogBaseDiagonalSpeed,	frogBaseDiagonalSpeed,	frogVerticalSpeed};
	struct Vector3 frogBaseVelocity2 = {	0,						frogBaseSpeed,			frogVerticalSpeed};
	struct Vector3 frogBaseVelocity3 = {	-frogBaseDiagonalSpeed, frogBaseDiagonalSpeed,	frogVerticalSpeed};
	struct Vector3 frogBaseVelocity4 = {	-frogBaseSpeed,			0,						frogVerticalSpeed};
	struct Vector3 frogBaseVelocity5 = {	-frogBaseDiagonalSpeed, -frogBaseDiagonalSpeed,	frogVerticalSpeed};
	struct Vector3 frogBaseVelocity6 = {	0,						-frogBaseSpeed,			frogVerticalSpeed};
	struct Vector3 frogBaseVelocity7 = {	frogBaseDiagonalSpeed,	-frogBaseDiagonalSpeed,	frogVerticalSpeed};
    frogBaseVelocities[0] = frogBaseVelocity0;
    frogBaseVelocities[1] = frogBaseVelocity1;
    frogBaseVelocities[2] = frogBaseVelocity2;
    frogBaseVelocities[3] = frogBaseVelocity3;
    frogBaseVelocities[4] = frogBaseVelocity4;
    frogBaseVelocities[5] = frogBaseVelocity5;
    frogBaseVelocities[6] = frogBaseVelocity6;
    frogBaseVelocities[7] = frogBaseVelocity7;
    
    // Bullets
    for (i = 0; i < MAX_FAIRY_BULLETS; i++)
    {
    	OBJ_ATTR(OBJ_PLAYER_BULLET(i), 0) = ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(160);
    	OBJ_ATTR(OBJ_PLAYER_BULLET(i), 1) = ATTR1_SIZE1 | ATTR1_X(240);
    	OBJ_ATTR(OBJ_PLAYER_BULLET(i), 2) = ATTR2_PALBANK(PALBANK_ICE) | ATTR2_PRIO(1) | ATTR2_ID(OBJ_BASEINDEX_BULLET);
    }
    for (i = 0; i < MAX_FROG_BULLETS; i++)
    {
    	OBJ_ATTR(OBJ_FROG_BULLET(i), 0) = ATTR0_SQUARE | ATTR0_4BPP | ATTR0_Y(160);
    	OBJ_ATTR(OBJ_FROG_BULLET(i), 1) = ATTR1_SIZE1 | ATTR1_X(240);
    	OBJ_ATTR(OBJ_FROG_BULLET(i), 2) = ATTR2_PALBANK(PALBANK_YELLOW) | ATTR2_PRIO(1) | ATTR2_ID(OBJ_BASEINDEX_BULLET);
    }
    
    // Attack (circle/explosion) shall be set on use as its uses are varied
    
    // Start the first level
    LoadLevel(0);
}
// END DataSetup

// -----------------------------------------------------------------------------
// Start menu
// -----------------------------------------------------------------------------

// object defines specifically for start menu
#define OBJ_SPLASH_START0 0
#define OBJ_SPLASH_START1 1
#define OBJ_SPLASH_START2 2
#define OBJ_SPLASH_START3 3

void SplashSetup()
{
	// Loop iterators
	int i;
    
    // Hide all sprites
    for (i = 0; i < 128; i++)
    {
        HideSpriteOffscreen(i);
    }
    
    // "Press START" text
	OBJ_ATTR(OBJ_SPLASH_START0, 0) = ATTR0_WIDE | ATTR0_Y(100);
	OBJ_ATTR(OBJ_SPLASH_START0, 1) = ATTR1_SIZE2 | ATTR1_X(56);
	OBJ_ATTR(OBJ_SPLASH_START0, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_PRESS_START0);
	OBJ_ATTR(OBJ_SPLASH_START1, 0) = ATTR0_WIDE | ATTR0_Y(100);
	OBJ_ATTR(OBJ_SPLASH_START1, 1) = ATTR1_SIZE2 | ATTR1_X(88);
	OBJ_ATTR(OBJ_SPLASH_START1, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_PRESS_START1);
	OBJ_ATTR(OBJ_SPLASH_START2, 0) = ATTR0_WIDE | ATTR0_Y(100);
	OBJ_ATTR(OBJ_SPLASH_START2, 1) = ATTR1_SIZE2 | ATTR1_X(120);
	OBJ_ATTR(OBJ_SPLASH_START2, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_PRESS_START2);
	OBJ_ATTR(OBJ_SPLASH_START3, 0) = ATTR0_WIDE | ATTR0_Y(100);
	OBJ_ATTR(OBJ_SPLASH_START3, 1) = ATTR1_SIZE2 | ATTR1_X(152);
	OBJ_ATTR(OBJ_SPLASH_START3, 2) = ATTR2_PALBANK(PALBANK_UI) | ATTR2_PRIO0 | ATTR2_ID(OBJ_BASEINDEX_PRESS_START3);
}
