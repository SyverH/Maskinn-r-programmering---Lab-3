#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <poll.h>

#include <dirent.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <sys/mman.h>

struct input_event ev;

// Initialises fd as an error
int fd = -1;

int frameBufferfd = -1;


// The game state can be used to detect what happens on the playfield
#define GAMEOVER   0
#define ACTIVE     (1 << 0)
#define ROW_CLEAR  (1 << 1)
#define TILE_ADDED (1 << 2)

typedef struct {
  uint16_t pixel[8][8]; // 8x8 pixel array
}pixeltype;

pixeltype* pixelarray;

// If you extend this structure, either avoid pointers or adjust
// the game logic allocate/deallocate and reset the memory
typedef struct {
  bool occupied;
  uint16_t color;
} tile;

typedef struct {
  unsigned int x;
  unsigned int y;
} coord;

typedef struct {
  coord const grid;                     // playfield bounds
  unsigned long const uSecTickTime;     // tick rate
  unsigned long const rowsPerLevel;     // speed up after clearing rows
  unsigned long const initNextGameTick; // initial value of nextGameTick

  unsigned int tiles; // number of tiles played
  unsigned int rows;  // number of rows cleared
  unsigned int score; // game score
  unsigned int level; // game level

  tile *rawPlayfield; // pointer to raw memory of the playfield
  tile **playfield;   // This is the play field array
  unsigned int state;
  coord activeTile;                       // current tile

  unsigned long tick;         // incremeted at tickrate, wraps at nextGameTick
                              // when reached 0, next game state calculated
  unsigned long nextGameTick; // sets when tick is wrapping back to zero
                              // lowers with increasing level, never reaches 0
} gameConfig;



gameConfig game = {
                   .grid = {8, 8},
                   .uSecTickTime = 10000,
                   .rowsPerLevel = 2,
                   .initNextGameTick = 50,
};

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

void drawPixel(uint8_t x, uint8_t y, uint16_t color) {
  pixelarray->pixel[x][y] = color;
}

void fillPixelArray(uint16_t color) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j <8; j++) {
      pixelarray->pixel[i][j] = color;
    }
  }
}

// This function is called on the start of your application
// Here you can initialize what ever you need for your task
// return false if something fails, else true
bool initializeSenseHat() {
  
  struct timeval timeout;
  struct dirent **subDirectoryNameList;
  struct dirent **frameBuffersubDirectoryNameList;

  char path[64];
  char frameBufferpath[64];

  bool deviceLocated = false;
  bool frameBufferDeviceLocated = false;

  // This loop will scan the /dev/input directory for devices, allowing all devices through
  int numLoops = scandir("/dev/input", &subDirectoryNameList, NULL, alphasort);
  int numFramebufferLoops = scandir("/dev", &frameBuffersubDirectoryNameList, NULL, alphasort);

  // Hvis scandir feiler, avslutt
  if (numLoops < 0) {
    perror("scandir failed!");
    return false;
  }

  if (numFramebufferLoops < 0) {
    perror("scandir failed!");
    return false;
  }

  for (int i = 0; i < numFramebufferLoops; i++) {
    if (strstr(frameBuffersubDirectoryNameList[i]->d_name, "fb")) {
      sprintf(frameBufferpath, "/dev/%s", frameBuffersubDirectoryNameList[i]->d_name);
      printf("the path is: %s\n", frameBufferpath);
      frameBufferfd = open(frameBufferpath, O_RDWR);

      printf("the fd is: %d\n", frameBufferfd);

      if (frameBufferfd != -1) {
        // Initialiserer deviceName som et char array der jeg kan lagre navnet på enheten som leses
        char frameBufferdeviceName[256];
        ioctl(frameBufferfd, FBIOGET_FSCREENINFO, frameBufferdeviceName);

        printf("the deviceName is: %s\n", frameBufferdeviceName);
        // Hvis enheten er funnet, avslutt
        if (strstr(frameBufferdeviceName, "RPi-Sense FB")) {
          printf("Sense HAT Framebuffer funnet\n");
          frameBufferDeviceLocated = true;
          pixelarray = mmap(NULL, 128, PROT_READ | PROT_WRITE, MAP_SHARED, frameBufferfd, 0);
          break;
        }
      }
      close(frameBufferfd);
  }
  free(frameBuffersubDirectoryNameList[i]);
  }
  free(frameBuffersubDirectoryNameList);

  if(!frameBufferDeviceLocated){
    fprintf(stderr, "ERROR: could not locate Sense HAT Framebuffer\n");
    return false;
  }

  if(frameBufferDeviceLocated){
    fprintf(stdout, "INFO: using Sense HAT Framebuffer at %s\n", frameBufferpath);
  }


  for (int i = 0; i < numLoops; i++) {
    if (strstr(subDirectoryNameList[i]->d_name, "event")) {
      sprintf(path, "/dev/input/%s", subDirectoryNameList[i]->d_name);
      
      fd = open(path, O_RDONLY);

      if (fd != -1) {
        // Initialiserer deviceName som et char array der jeg kan lagre navnet på enheten som leses
        char deviceName[256];
        ioctl(fd, EVIOCGNAME(sizeof(deviceName)), deviceName);

        // Hvis enheten er funnet, avslutt
        if (strstr(deviceName, "Raspberry Pi Sense HAT Joystick")) {
          deviceLocated = true;
          break;
        }
      }
      close(fd);
    }
    // Gi slipp på minne som er allokert av scandir()
    free(subDirectoryNameList[i]);
  }
  // Gi slipp på minne som er allokert av scandir()
  free(subDirectoryNameList);

  // Hvis enhet ikke er funnet, avslutt
  if (!deviceLocated) {
    fprintf(stderr, "ERROR: could not locate Sense HAT Joystick\n");
    return false;
  }

  // Hvis enhet er funnet, skriv ut path til enhet
  if(deviceLocated){
    fprintf(stdout, "INFO: using Sense HAT Joystick at %s\n", path);
  }
  return true;
}

// This function is called when the application exits
// Here you can free up everything that you might have opened/allocated
void freeSenseHat() {
  close(fd);
  close(frameBufferfd);
  munmap(pixelarray, 128);
}

// This function should return the key that corresponds to the joystick press
// KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, with the respective direction
// and KEY_ENTER, when the the joystick is pressed
// !!! when nothing was pressed you MUST return 0 !!!
int readSenseHatJoystick() {

  struct pollfd pfd;

  pfd.fd = fd;
  pfd.events = POLLIN;

  if (poll(&pfd, 1, 0) == -1) {
        perror("Poll-feil");
        close(fd);
        return 0;
    }

  if (pfd.revents & POLLIN) {
        if (read(fd, &ev, sizeof(struct input_event)) == -1) {
            perror("Lesing av hendelse feilet");
            close(fd);
            return 0;
        }
  }

  // Begrenser hvilke event-types som returneres
  if (ev.type == EV_KEY && (ev.code == KEY_UP || ev.code == KEY_DOWN || ev.code == KEY_LEFT || ev.code == KEY_RIGHT || ev.code == KEY_ENTER)) {
      // Rising edge detector, så knappen ikke leses av flere ganger
      if (ev.value == 1) {
        // Returnerer event-koden til knapp som er trykket
        return ev.code;
      }
      else if (ev.value == 2) {
        // Returnerer event-koden til knapp som er holdt inne
        return ev.code;
      }
  }
  return 0;
}


// This function should render the gamefield on the LED matrix. It is called
// every game tick. The parameter playfieldChanged signals whether the game logic
// has changed the playfield
void renderSenseHatMatrix(bool const playfieldChanged) {
  if (!playfieldChanged){
    return;
  }
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j <8; j++) {
      drawPixel(i, j, game.playfield[i][j].color);
    }
  }
}


// The game logic uses only the following functions to interact with the playfield.
// if you choose to change the playfield or the tile structure, you might need to
// adjust this game logic <> playfield interface

static inline void newTile(coord const target) {
  game.playfield[target.y][target.x].occupied = true;
  game.playfield[target.y][target.x].color = rgb565(rand() % 0xffff, rand() % 0xffff, rand() % 0xffff);
}

static inline void copyTile(coord const to, coord const from) {
  memcpy((void *) &game.playfield[to.y][to.x], (void *) &game.playfield[from.y][from.x], sizeof(tile));
}

static inline void copyRow(unsigned int const to, unsigned int const from) {
  memcpy((void *) &game.playfield[to][0], (void *) &game.playfield[from][0], sizeof(tile) * game.grid.x);

}

static inline void resetTile(coord const target) {
  memset((void *) &game.playfield[target.y][target.x], 0, sizeof(tile));
}

static inline void resetRow(unsigned int const target) {
  memset((void *) &game.playfield[target][0], 0, sizeof(tile) * game.grid.x);
}

static inline bool tileOccupied(coord const target) {
  return game.playfield[target.y][target.x].occupied;
}

static inline bool rowOccupied(unsigned int const target) {
  for (unsigned int x = 0; x < game.grid.x; x++) {
    coord const checkTile = {x, target};
    if (!tileOccupied(checkTile)) {
      return false;
    }
  }
  return true;
}


static inline void resetPlayfield() {
  for (unsigned int y = 0; y < game.grid.y; y++) {
    resetRow(y);
  }
}

// Below here comes the game logic. Keep in mind: You are not allowed to change how the game works!
// that means no changes are necessary below this line! And if you choose to change something
// keep it compatible with what was provided to you!

bool addNewTile() {
  game.activeTile.y = 0;
  game.activeTile.x = (game.grid.x - 1) / 2;
  if (tileOccupied(game.activeTile))
    return false;
  newTile(game.activeTile);
  return true;
}

bool moveRight() {
  coord const newTile = {game.activeTile.x + 1, game.activeTile.y};
  if (game.activeTile.x < (game.grid.x - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool moveLeft() {
  coord const newTile = {game.activeTile.x - 1, game.activeTile.y};
  if (game.activeTile.x > 0 && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool moveDown() {
  coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
  if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool clearRow() {
  if (rowOccupied(game.grid.y - 1)) {
    for (unsigned int y = game.grid.y - 1; y > 0; y--) {
      copyRow(y, y - 1);
    }
    resetRow(0);
    return true;
  }
  return false;
}

void advanceLevel() {
  game.level++;
  switch(game.nextGameTick) {
  case 1:
    break;
  case 2 ... 10:
    game.nextGameTick--;
    break;
  case 11 ... 20:
    game.nextGameTick -= 2;
    break;
  default:
    game.nextGameTick -= 10;
  }
}

void newGame() {
  game.state = ACTIVE;
  game.tiles = 0;
  game.rows = 0;
  game.score = 0;
  game.tick = 0;
  game.level = 0;
  resetPlayfield();
}

void gameOver() {
  game.state = GAMEOVER;
  game.nextGameTick = game.initNextGameTick;
}


bool sTetris(int const key) {
  bool playfieldChanged = false;

  if (game.state & ACTIVE) {
    // Move the current tile
    if (key) {
      playfieldChanged = true;
      switch(key) {
      case KEY_LEFT:
        moveLeft();
        break;
      case KEY_RIGHT:
        moveRight();
        break;
      case KEY_DOWN:
        while (moveDown()) {};
        game.tick = 0;
        break;
      default:
        playfieldChanged = false;
      }
    }

    // If we have reached a tick to update the game
    if (game.tick == 0) {
      // We communicate the row clear and tile add over the game state
      // clear these bits if they were set before
      game.state &= ~(ROW_CLEAR | TILE_ADDED);

      playfieldChanged = true;
      // Clear row if possible
      if (clearRow()) {
        game.state |= ROW_CLEAR;
        game.rows++;
        game.score += game.level + 1;
        if ((game.rows % game.rowsPerLevel) == 0) {
          advanceLevel();
        }
      }

      // if there is no current tile or we cannot move it down,
      // add a new one. If not possible, game over.
      if (!tileOccupied(game.activeTile) || !moveDown()) {
        if (addNewTile()) {
          game.state |= TILE_ADDED;
          game.tiles++;
        } else {
          gameOver();
        }
      }
    }
  }

  // Press any key to start a new game
  if ((game.state == GAMEOVER) && key) {
    playfieldChanged = true;
    newGame();
    addNewTile();
    game.state |= TILE_ADDED;
    game.tiles++;
  }

  return playfieldChanged;
}

int readKeyboard() {
  struct pollfd pollStdin = {
       .fd = STDIN_FILENO,
       .events = POLLIN
  };
  int lkey = 0;

  if (poll(&pollStdin, 1, 0)) {
    lkey = fgetc(stdin);
    if (lkey != 27)
      goto exit;
    lkey = fgetc(stdin);
    if (lkey != 91)
      goto exit;
    lkey = fgetc(stdin);
  }
 exit:
    switch (lkey) {
      case 10: return KEY_ENTER;
      case 65: return KEY_UP;
      case 66: return KEY_DOWN;
      case 67: return KEY_RIGHT;
      case 68: return KEY_LEFT;
    }
  return 0;
}

void renderConsole(bool const playfieldChanged) {
  if (!playfieldChanged)
    return;

  // Goto beginning of console
  fprintf(stdout, "\033[%d;%dH", 0, 0);
  for (unsigned int x = 0; x < game.grid.x + 2; x ++) {
    fprintf(stdout, "-");
  }
  fprintf(stdout, "\n");
  for (unsigned int y = 0; y < game.grid.y; y++) {
    fprintf(stdout, "|");
    for (unsigned int x = 0; x < game.grid.x; x++) {
      coord const checkTile = {x, y};
      fprintf(stdout, "%c", (tileOccupied(checkTile)) ? '#' : ' ');
    }
    switch (y) {
      case 0:
        fprintf(stdout, "| Tiles: %10u\n", game.tiles);
        break;
      case 1:
        fprintf(stdout, "| Rows:  %10u\n", game.rows);
        break;
      case 2:
        fprintf(stdout, "| Score: %10u\n", game.score);
        break;
      case 4:
        fprintf(stdout, "| Level: %10u\n", game.level);
        break;
      case 7:
        fprintf(stdout, "| %17s\n", (game.state == GAMEOVER) ? "Game Over" : "");
        break;
    default:
        fprintf(stdout, "|\n");
    }
  }
  for (unsigned int x = 0; x < game.grid.x + 2; x++) {
    fprintf(stdout, "-");
  }
  fflush(stdout);
}


inline unsigned long uSecFromTimespec(struct timespec const ts) {
  return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  // This sets the stdin in a special state where each
  // keyboard press is directly flushed to the stdin and additionally
  // not outputted to the stdout
  {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
  }

  // Allocate the playing field structure
  game.rawPlayfield = (tile *) malloc(game.grid.x * game.grid.y * sizeof(tile));
  game.playfield = (tile**) malloc(game.grid.y * sizeof(tile *));
  if (!game.playfield || !game.rawPlayfield) {
    fprintf(stderr, "ERROR: could not allocate playfield\n");
    return 1;
  }
  for (unsigned int y = 0; y < game.grid.y; y++) {
    game.playfield[y] = &(game.rawPlayfield[y * game.grid.x]);
  }

  // Reset playfield to make it empty
  resetPlayfield();
  // Start with gameOver
  gameOver();

  if (!initializeSenseHat()) {
    fprintf(stderr, "ERROR: could not initilize sense hat\n");
    return 1;
  };

  // Clear console, render first time
  fprintf(stdout, "\033[H\033[J");
  renderConsole(true);
  renderSenseHatMatrix(true);

  while (true) {
    struct timeval sTv, eTv;
    gettimeofday(&sTv, NULL);

    int key = readSenseHatJoystick();

    if (!key)
      key = readKeyboard();
    if (key == KEY_ENTER){
      printf("ENTER\n");
      break;
    }

    bool playfieldChanged = sTetris(key);
    renderConsole(playfieldChanged);
    renderSenseHatMatrix(playfieldChanged);

    // Wait for next tick
    gettimeofday(&eTv, NULL);
    unsigned long const uSecProcessTime = ((eTv.tv_sec * 1000000) + eTv.tv_usec) - ((sTv.tv_sec * 1000000 + sTv.tv_usec));
    if (uSecProcessTime < game.uSecTickTime) {
      usleep(game.uSecTickTime - uSecProcessTime);
    }
    game.tick = (game.tick + 1) % game.nextGameTick;
  }

  freeSenseHat();
  free(game.playfield);
  free(game.rawPlayfield);

  return 0;
}