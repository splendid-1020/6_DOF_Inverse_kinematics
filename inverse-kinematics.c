#include <stdio.h>    // Standard input/output definitions
#include <string.h>
#include <stdlib.h>
#include <unistd.h>   // UNIX standard function definitions
#include <fcntl.h>    // File control definitions
#include <errno.h>    // Error number definitions
#include <termios.h>  // POSIX terminal control definitions
#include <string.h>   // String function definitions
#include <sys/ioctl.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include "inverse-kinematics.h"
#include "arduino-serial-lib.h"

#define d1  10.2   //ground to q1
#define d6  12.6 //gripper to wrist
#define a_2 14    //q1 to q2
#define a_3 14.2  //q2 to wrist
#define pi  3.14159265358979323846264338327950288419716939937510
#define degtorad 0.0174532925
#define radtodeg 57.29577951
int ticks[7]; /* pwm signals for the servos */
/* arrays for the multimap function which makes the servos a bit more linear */
long double inangles[5]      =  {0,pi/4.0, pi/2.0, (3.0/4.0)*pi, pi};
long double servovals[7][5] =   {{135,245,355,465,577}, //0
                                 {150,265,370,475,590}, //1
                                 {167,285,385,490,650}, //2
                                 {110,210,315,440,560}, //3
                                 {100,215,320,425,550}, //4
                                 {115,235,345,460,565}, //5
                                 {160,270,370,470,580}};//6

struct Pos{
    long double x; long double y; long double z;
    long double alpha; long double beta; long double gamma;
};


void circle(long double start[3][4],long double centre[3],long double part,int direction){
    /* work in progress */
}

void sendStuff(int fd, short val){
    uint8_t MSB;
    uint8_t LSB;
    LSB = val & 0xff;
    MSB = (val >> 8) &0xff;
    serialport_writebyte(fd,MSB);
    serialport_writebyte(fd,LSB);
}

void commandArduino(int fd, double angles[7]){
    int j;
    ticks[0] = getServoTick(angles[1],0);
    ticks[1] = getServoTick(angles[2],1);
    ticks[2] = getServoTick(angles[2],2);
    ticks[3] = getServoTick(angles[3],3);
    ticks[4] = getServoTick(angles[4],4);
    ticks[5] = getServoTick(angles[5],5);
    ticks[6] = getServoTick(angles[6],6);
    for(j=0; j<7; j++){
        sendStuff(fd,ticks[j]);
    }
}

int getServoTick(float val, int servoNumber){ /* a multimap from angles to servo signals */
  int size = 5;
  // take care the value is within range
  // val = constrain(val, _in[0], _in[size-1]);
  if (val <= inangles[0]) return servovals[servoNumber][0];
  if (val >= inangles[size-1]) return servovals[servoNumber][size-1];

  // search right interval
  int pos = 1;  // _in[0] allready tested
  while(val > inangles[pos]) pos++;

  // this will handle all exact "points" in the _in array
  if (val == inangles[pos]) return servovals[servoNumber][pos];

  // interpolate in the right segment for the rest
  return roundl((val - inangles[pos-1]) * (servovals[servoNumber][pos] - servovals[servoNumber][pos-1]) / (inangles[pos] - inangles[pos-1]) + servovals[servoNumber][pos-1]);
}

void inverseKinematics(double x,double y,double z,double t[3][3],double angles[7]) /*results are stored in angles array */
{
    /* the method here follows the calculations from the book:
    Modelling and Control of Robot Manipulators, Lorenzo Sciavicco and Bruno Siciliano chapter 2 (specifically 2.12.2)*/
    double xc,yc,zc;
    double ay,ax,az,sz,nz;
    double r11,r12,r13,r21,r22,r23,r31,r32,r33;
    double q1,q2,q3;
    double D,k1,k2;
    /* compute the wrist position */
    xc = x - d6*t[0][2];
    yc = y - d6*t[1][2];
    zc = z - d6*t[2][2];
    /* solve inverse kinematics for the first 3 angles found in: */
    /* http://www.hessmer.org/uploads/RobotArm/Inverse%2520Kinematics%2520for%2520Robot%2520Arm.pdf */
    D = ( pow(xc,2) + pow(yc,2) + pow((zc-d1),2) - pow(a_2,2) - pow(a_3,2) )/(2*a_2*a_3);
    angles[1] = atan2l(yc,xc);
    angles[3] = atan2l( -sqrt( 1 - pow(D,2) ),D );
    k1 = a_2+a_3*cosl(angles[3]);
    k2 = a_3*sinl(angles[3]);
    angles[2] = atan2l( (zc-d1), sqrt(pow(xc,2) + pow(yc,2)) ) - atan2l(k2,k1) ;
    /* the DH frame is rotated 90 degrees compared to the calculated value see 2.9.7 from the book*/
    angles[3] = angles[3] + pi/2;

    /* for my own sanity */
    q1=angles[1]; q2=angles[2]; q3=angles[3];
    r11=t[0][0];r12=t[0][1];r13=t[0][2]; r21=t[1][0];r22=t[1][1];r23=t[1][2]; r31=t[2][0];r32=t[2][1];r33=t[2][2];

    /* solve inverse kinematics for the final 3 angles (2.12.5 from the book) */
    ax = r13*cosl(q1)*cosl(q2 + q3) + r23*cosl(q2 + q3)*sinl(q1) + r33*sinl(q2 + q3);
    ay = -r23*cosl(q1) + r13*sinl(q1);
    az = -r33*cosl(q2 + q3) + r13*cosl(q1)*sinl(q2 + q3) + r23*sinl(q1)*sinl(q2 + q3);
    sz = -r32*cosl(q2 + q3) + r12*cosl(q1)*sinl(q2 + q3) + r22*sinl(q1)*sinl(q2 + q3);
    nz = -r31*cosl(q2 + q3) + r11*cosl(q1)*sinl(q2 + q3) + r21*sinl(q1)*sinl(q2 + q3);

    /* getting the angles from the matrix only works if ax != 0 and ay != 0 */
    if(ax == 0)
        ax += epsilon;
    if(ay == 0)
        ay += epsilon;

    angles[4] = atan2l(-ay,-ax);
    angles[5] = atan2l(-sqrt(ax*ax+ay*ay),az);
    angles[6] = atan2l(-sz,nz);

    /* all that follows now is fixing the angles because some of the servo orientations */
    /* do no align with the DH frames and servo's can only move 180 degrees*/
    if (angles[4] > pi/2){
    angles[4] = angles[4] - pi;
    angles[5] = -angles[5];
    angles[6] = pi + angles[6];
    }
    if (angles[4] < -pi/2){
    angles[4] = angles[4] + pi;
    angles[5] = -angles[5];
    angles[6] = pi + angles[6];
    }
    angles[1] = angles[1];
    angles[2] = angles[2];
    angles[3] = -(angles[3] - pi/2);
    angles[4] = (angles[4] + pi/2);
    angles[5] = (pi/2) - angles[5];
    angles[6] = fmod( ((pi/2) + angles[6] ), 2.0*pi) ;
}
