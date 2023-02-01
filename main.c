#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <p32xxxx.h>
#include <xc.h>
#include <sys/attribs.h>
#include <time.h>
#include "config.h"
#include "lcd.h"
#include "audio.h"
#include "ssd.h"
#include "utils.h"
#include "rgbled.h"
#include "led.h"

#pragma config JTAGEN   = OFF
#pragma config FWDTEN   = OFF
#pragma config FNOSC    = FRCPLL
#pragma config FSOSCEN  = OFF
#pragma config POSCMOD  = EC
#pragma config OSCIOFNC = ON
#pragma config FPBDIV   = DIV_1
#pragma config FPLLIDIV = DIV_2
#pragma config FPLLMUL  = MUL_20
#pragma config FPLLODIV = DIV_1

#define SW0 PORTFbits.RF3
#define SIZE 16
#define ROW_SIZE 2
#define PLAYER_POS 0
#define LIVES 8
#define COIN_CODE 1
#define OBSTACLE_CODE 2
#define RESTART_KEY '1'
#define START_TOP_POS 0x80
#define START_BOTTOM_POS 0xC0

void initController();
void initLCD();

void setSpaceshipLocation();
int checkTopOrBottom();

void busy(void);

void printObjectsToScreen();
void moveObjects();
int getRandomObjectNumber();
void checkCollision();
void hitSound();

void showGameOver();
void playSound();

char getNumberFromKeyboard();
int in_y(int);
char numberReturnValue(int);

void restartGame();


int arr[ROW_SIZE][SIZE] = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
int player_pos;
int hit_flag = 0;
int counter = 0;
int life = LIVES;

void main() {
  initController();
  AUDIO_Init(0);
  AUDIO_Close();
  start:
    srand(time(0));
  initLCD();
  LED_Init();
  LED_SetGroupValue(0xffff);
  SSD_Init(0);
  //main game loop
  while (life) {
    setSpaceshipLocation();
    printObjectsToScreen();
    moveObjects();
    checkCollision();
    busy();
    initLCD();
    setSpaceshipLocation();
    int k;
    for (k = 0; k < 32000; k++);
    SSD_WriteDigitsGrouped(counter, 0);
  }
  showGameOver();
  //"Game over" loop. Exiting this loop using the "start" label with "goto" asm operation
  while (1) {
    playSound();
    char key = getNumberFromKeyboard();
    if (key == RESTART_KEY) {
      hitSound();
      restartGame();
      goto start;
    }
  }

}

/*
 *  This function initializes all the inputs and
 *  outputs that are used to run the program on the controller.
 */
void initController() {
  TRISCbits.TRISC2 = 0; //RC2
  TRISCbits.TRISC1 = 0; //RC1
  TRISCbits.TRISC4 = 0; //RC4            
  TRISGbits.TRISG6 = 0; //RG6
  ANSELGbits.ANSG6 = 0; //Output as digital
  TRISCbits.TRISC3 = 1; //RC3
  CNPUCbits.CNPUC3;
  TRISGbits.TRISG7 = 1; //RG7
  ANSELGbits.ANSG7 = 0;
  CNPUGbits.CNPUG7;
  TRISGbits.TRISG8 = 1; //RG8
  ANSELGbits.ANSG8 = 0;
  CNPUGbits.CNPUG8;
  TRISGbits.TRISG9 = 1; //RG9
  ANSELGbits.ANSG9 = 0;
  CNPUGbits.CNPUG9;
  TRISFbits.TRISF3 = 1; // RF3 (SW0) configured as input
}

/*
 *  This function initializes the LCD component
 */
void initLCD() {
  LCD_Init();
  TRISE &= 0xff00;
  TRISBbits.TRISB15 = 0; // RB15 (DISP_RS) set as an output
  ANSELBbits.ANSB15 = 0; // disable analog functionality on RB15 (DISP_RS)
  TRISDbits.TRISD5 = 0; // RD5 (DISP_RW) set as an output
  TRISDbits.TRISD4 = 0; // RD4 (DISP_EN) set as an output
  ANSELEbits.ANSE2 = 0;
  ANSELEbits.ANSE4 = 0;
  ANSELEbits.ANSE5 = 0;
  ANSELEbits.ANSE6 = 0;
  ANSELEbits.ANSE7 = 0;
  PORTBbits.RB15 = 0; //rs=0
  PORTDbits.RD5 = 0; //w=0

  char control[7] = {
    0x38,
    0x38,
    0x38,
    0xe,
    0x6,
    0x1,
    0x40
  };
  char strLCD[8] = {
    0x58,
    0x5C,
    0x4E,
    0x5F,
    0x5F,
    0x4E,
    0x5C,
    0x58
  };
  int controlSize = sizeof(control) / sizeof(char);
  int strLCDSize = sizeof(strLCD) / sizeof(char);
  int i;
  for (i = 0; i < controlSize; i++) {
    PORTE = control[i];
    PORTDbits.RD4 = 1;
    PORTDbits.RD4 = 0;
    busy();
  }
  PORTBbits.RB15 = 1; //rs
  for (i = 0; i < strLCDSize; i++) {
    PORTE = strLCD[i];
    PORTDbits.RD4 = 1; //enable=1
    PORTDbits.RD4 = 0; //enable=0
    busy();
  }
  PORTBbits.RB15 = 0; //rs control 
  PORTE = 0x80; //DDRAM
  PORTDbits.RD4 = 1; //enable=1
  PORTDbits.RD4 = 0; //enable=0
  busy();
}

/*
 *  This function checks and sets the spaceship's location
 */
void setSpaceshipLocation() {
  PORTBbits.RB15 = 0;
  PORTE = checkTopOrBottom();
  PORTDbits.RD4 = 1;
  PORTDbits.RD4 = 0;
  busy();
  PORTBbits.RB15 = 1; //rs 
  PORTE = 0;
  PORTDbits.RD4 = 1; //enable=1
  PORTDbits.RD4 = 0; //enable=0
  busy();
}

/*
 *  This function return the position of the spaceship
 *  player_pos: 0 is for 0x80 and 1 is for 0xC0
 */
int checkTopOrBottom() {
  player_pos = SW0 ? 0 : 1;
  return player_pos ? START_TOP_POS : START_BOTTOM_POS;
}

/*
 *  This function creates a delay until the data to the LCD component
 *  was transfered
 */
void busy(void) {
  char RD, RS;
  int STATUS_TRISE;
  int portMap;
  RD = PORTDbits.RD5;
  RS = PORTBbits.RB15;
  STATUS_TRISE = TRISE;
  PORTDbits.RD5 = 1; //w/r
  PORTBbits.RB15 = 0; //rs 
  portMap = TRISE;
  portMap |= 0x80;
  TRISE = portMap;
  do {
    PORTDbits.RD4 = 1; //enable=1
    PORTDbits.RD4 = 0; //enable=0
  }
  while (PORTEbits.RE7); // BF
  PORTDbits.RD5 = RD;
  PORTBbits.RB15 = RS;
  TRISE = STATUS_TRISE;
}

/*
 *  This function prints the objects of the game:
 *  "*" - generates an obstacle
 *  "O" - generates a reward
 *  " " - generates an empty cell
 */
void printObjectsToScreen() {
  int i, j;
  char selectedObject = "";
  for (i = 0; i < ROW_SIZE; i++) {
    for (j = 1; j < SIZE; j++) {
      if (arr[i][j] == OBSTACLE_CODE)
        selectedObject = "*";
      else if (arr[i][j] == COIN_CODE)
        selectedObject = "O";
      else
        selectedObject = " ";
      LCD_WriteStringAtPos(selectedObject, i, j);
    }
    DelayAprox10Us(10000);
  }
}

/*
 *  This function holds the logic of the object's movement.
 *  A new random position for every obstacle is created
 */
void moveObjects() {
  int i, j;
  for (i = 0; i < ROW_SIZE; i++)
    for (j = 1; j < SIZE - 1; j++)
      arr[i][j] = arr[i][j + 1];
  int num = getRandomObjectNumber();
  //mod 2 prints only on 1 line, mod 3 prints on both lines
  int position = rand() % 3;
  arr[position][SIZE - 1] = num;
}

/*
 *  This function generates a new number
 *  using a random seed
 */
int getRandomObjectNumber() {
  int r = rand() % 101;
  if (r >= 80 && r < 90)
    return OBSTACLE_CODE;
  else if (r > 90 && r <= 100)
    return COIN_CODE;
  else return 0;
}

/*
 *  This function checks instantly if
 *  the spaceship hits an obstacle or a reward
 */
void checkCollision() {
  int i;
  for (i = 0; i < ROW_SIZE; i++) {
    int spaceshipPos = arr[i][1];
    if (spaceshipPos == COIN_CODE && player_pos == i) {
      hit_flag = 1;
      counter++;
      if (life < LIVES) {
        life++;
        LED_SetValue(LIVES - life, 1);
      }
    } else if (spaceshipPos == OBSTACLE_CODE && player_pos == i) {
      if (life > 0) {
        life--;
        hitSound();
        LED_SetValue(LIVES - life - 1, 0);
      }
    }
  }
}

/*
 *  This function creates a beep sound for hitting an obstacle
 */
void hitSound() {
  T3CONbits.ON = 1;
  OC1CONbits.ON = 1;
  DelayAprox10Us(1000);
  T3CONbits.ON = 0;
  OC1CONbits.ON = 0;
}

/*
 *  This function prints to the LCD "Game Over 1 to restart"
 */
void showGameOver() {
  LCD_WriteStringAtPos("  Game Over", 0, 0);
  LCD_WriteStringAtPos("  1 to restart ", 1, 0);
}

/*
 *  This sound is triggered if the game is over
 */
void playSound() {
  T3CONbits.ON = 0;
  OC1CONbits.ON = 0;
  DelayAprox10Us(5000);
  T3CONbits.ON = 1;
  OC1CONbits.ON = 1;
  DelayAprox10Us(9000);
  T3CONbits.ON = 0;
  OC1CONbits.ON = 0;
}

/*
 *  This function identifies and returns which keyboard key was pressed
 */
char getNumberFromKeyboard() {
  int xy;
  int i;
  i = 1;

  PORTCbits.RC2 = 1;
  PORTCbits.RC1 = 1;
  PORTCbits.RC4 = 1;
  PORTGbits.RG6 = 1;

  i = 1;
  PORTCbits.RC2 = 0;
  xy = in_y(1);
  if (xy != 0xff)
    return numberReturnValue(xy);
  PORTCbits.RC2 = 1;
  i = 2;
  PORTCbits.RC1 = 0;
  xy = in_y(2);
  if (xy != 0xff)
    return numberReturnValue(xy);
  PORTCbits.RC1 = 1;
  i = 3;
  PORTCbits.RC4 = 0;
  xy = in_y(3);
  if (xy != 0xff)
    return numberReturnValue(xy);
  PORTCbits.RC4 = 1;
  i = 4;
  PORTGbits.RG6 = 0;
  xy = in_y(4);
  if (xy != 0xff)
    return numberReturnValue(xy);
  PORTGbits.RG6 = 1;

  return key;
}

int in_y(int a) {
  int j = 1, flag = 0;

  if (!PORTCbits.RC3) {
    flag = 1;
    j = 1;
  } else if (!PORTGbits.RG7) {
    flag = 1;
    j = 2;
  } else if (!PORTGbits.RG8) {
    flag = 1;
    j = 3;
  } else if (!PORTGbits.RG9) {
    flag = 1;
    j = 4;
  }
  if (flag == 0)
    return (0xff);
  else
    return (j | (a << 4));
}

char numberReturnValue(int xy) {
    char scan_key[]={0x44,'1',0x34,'2',0x24,'3',0x43,'4',0x33,'5',0x23,'6',0x42,'7',0x32,'8',0x22,'9',0x41,'0'};
    int i;
    for (i = 0; i < 20; i += 2)
        if (scan_key[i] == xy)
            return (char) scan_key[i + 1];
}

/*
 *  This function restarts the game by resetting
 *  the global parameters.
 */
void restartGame() {
  life = LIVES;
  counter = 0;
  key = 0;
  int i, j;
  for (i = 0; i < ROW_SIZE; i++) {
    for (j = 0; j < SIZE; j++) {
      arr[i][j] = 0;
    }
  }
}

