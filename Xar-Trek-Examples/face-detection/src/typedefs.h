/*===============================================================*/
/*                                                               */
/*                          typedefs.h                           */
/*                                                               */
/*                     Typedefs for the host                     */
/*                                                               */
/*===============================================================*/

#ifndef __HAAR_H__
#define __HAAR_H__

// constants
#define IMAGE_HEIGHT 240
#define IMAGE_WIDTH 320
#define RESULT_SIZE 100
#define IMAGE_MAXGREY 255
#define IMAGE_SIZE ( IMAGE_HEIGHT * IMAGE_WIDTH )
#define TOTAL_NODES 2913
#define TOTAL_STAGES 25
#define TOTAL_COORDINATES TOTAL_NODES * 12
#define TOTAL_WEIGHTS TOTAL_NODES * 3
#define WINDOW_SIZE 25
#define SQ_SIZE 2
#define PYRAMID_HEIGHT 12
#define ROWS 25
#define COLS 25
#define NUM_BANKS 12
#define SIZE 2913

// standard datatypes
typedef struct MyPoint
{
  int x;
  int y;
} MyPoint;

typedef struct
{
  int width;
  int height;
} MySize;

typedef struct
{
  int x; 
  int y;
  int width;
  int height;
} MyRect;

typedef struct 
{
  int width;
  int height;
  int maxgrey;
  int flag;
} MyInputImage;

#endif
