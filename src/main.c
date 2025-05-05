// -----------------------------------------------------------------------------
// C-Skeleton to be used with HAM Library from www.ngine.de
// -----------------------------------------------------------------------------
#pragma GCC optimize("O0")
#include "gba.h"
#include "game.h"

int InGame = FALSE;

void Handler(void)
{
    REG_IME = 0x0;		// Stop all other interrupt handling, while we handle this current one
    
    if ((REG_IF & INT_VBLANK) == INT_VBLANK)
    {
    	if (InGame)
    	{
    		// Run the frame-by-frame update
    		// If update returns 0, continue being in game, otherwise game over
    		int isGameOver = Update();
    		
    		if (isGameOver)
    		{
    			InGame = FALSE;
    			SplashSetup();		// Show start menu
    		}
        }
        else // !InGame
        {
        	// Check if the player wants to start the game
        	if (INPUT_ACTIVE(KEY_START))
        	{
        		GameSetup();		// Prepare data for game
        		InGame = TRUE;
        	}
        }
    }
    else if ((REG_IF & INT_TIMER3) == INT_TIMER3)
    {
    	OnTimerEnd();
    }
    
    REG_IF = REG_IF;	// (remove) Update interrupt table, to confirm we have handled this interrupt
    REG_IME = 0x1;		// Re-enable interrupt handling
}


// -----------------------------------------------------------------------------
// Project Entry Point
// -----------------------------------------------------------------------------
int main(void)
{
	REG_IME = 0x0;					// Master disable interrupts
	
	// Registers
	REG_IE = INT_VBLANK | INT_TIMER3;										// Enable interrupt sources
    REG_DISPCNT = MODE0 | OBJ_MAP_1D | OBJ_ENABLE | BG0_ENABLE;
    REG_DISPSTAT = 0x8;														// Vblank interrupt enable
    REG_BG0CNT = BG_PRIO3 | BG_8BPP | BG_CBB(0) | BG_SBB(8) | BG_REG_32x32;	// map
    REG_BG1CNT = BG_PRIO0 | BG_8BPP | BG_CBB(0) | BG_SBB(9) | BG_REG_32x32;	// pause menu
    
    REG_INT = (int)&Handler;		// Set interrupt handler function
	
	DataSetup();
	SplashSetup();
	
    REG_IME = 0x1;					// Master enable interrupts

    while(1);

	return 0;
}

