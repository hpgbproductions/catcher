// Set sprite and tile image data
void DataSetup(void);

// Set game data and starting object attributes
void GameSetup(void);

// Display the start screen
void SplashSetup(void);

// Conduct updates
// Returns 0 if the game should continue as normal, otherwise returns other numbers
int Update(void);

// Load a level by its index
void LoadLevel(int);

// -----------------------------------------------------------------------------
// Miscellaneous functions
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Game timer and its interrupts
// -----------------------------------------------------------------------------

int RemainingSeconds(void);

void PauseGameTimer(void);

void UnpauseGameTimer(void);

// Start a timer using a seconds timer TM2 and seconds counter TM3
void StartGameTimer(int seconds);

// TM3 interrupt
void OnTimerEnd(void);
