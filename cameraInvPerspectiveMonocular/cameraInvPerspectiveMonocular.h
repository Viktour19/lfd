/* 
  Example use of openCV to compute the inverse perspective transformation from a camera model
  -------------------------------------------------------------------------------------------
   
  (This is the interface file: it contains the declarations of dedicated functions to implement the application.
  These function are called by client code in the application file. The functions are defined in the implementation file.)

  David Vernon
  14 June 2018
*/
 

#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <ctype.h>
#include <iostream>
#include <string>
// #include <conio.h> 
#include <sys/types.h> 
#include <sys/timeb.h>

//opencv
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>


#define TRUE  1
#define FALSE 0
#define MAX_STRING_LENGTH 80
#define MAX_FILENAME_LENGTH 80

using namespace std;
using namespace cv;

struct imagePointType {
   int u, v;
};

struct worldPointType {
   float x, y, z;
};


/* function prototypes go here */

void inversePerspectiveTransformation(Point2f image_sample_point, float camera_model[][4], float z, Point3f *world_sample_point);
void getSamplePoint( int event, int x, int y, int, void*);
void prompt_and_exit(int status);
void prompt_and_continue();
void pause(int milliseconds);