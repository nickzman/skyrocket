#ifndef __SKYROCKET__
#define __SKYROCKET__

#include "flare.h"
#include "rsMath.h"
#include "rsText.h"
#include "smoke.h"
#include "SoundEngine.h"
#include <OpenGL/gl.h>

#define PI 3.14159265359f
#define PIx2 6.28318530718f
#define D2R 0.0174532925f
#define R2D 57.2957795131f
#define MAXFLARES 110    // Required 100 and 10 extra for good measure
#define MINDEPTH -1000000.0f  // particle depth for inactive particles

class World;
class particle;

typedef struct SkyrocketSaverSettings
{
	int readyToDraw/* = 0*/;
	
	// Window variables
	int xsize, ysize, centerx, centery;
	float aspectRatio;
	// Camera variables
	/*static */rsVec lookFrom[3];  // 3 = position, target position, last position
	/*static */rsVec lookAt[3]  // 3 = position, target position, last position
		/*= {rsVec(0.0f, 1000.0f, 0.0f), 
		rsVec(0.0f, 1000.0f, 0.0f), 
		rsVec(0.0f, 1000.0f, 0.0f)}*/;
	rsVec cameraPos;  // used for positioning sounds (same as lookFrom[0])
	rsVec cameraVel;  // used for doppler shift
					  // Mouse variables
	float mouseIdleTime;
	int mouseButtons, mousex, mousey;
	float mouseSpeed;
	
	// the sound engine
	//SoundEngine* soundengine/* = NULL*/;
	
	// flare display lists
	unsigned int flarelist[4];
	// matrix junk for drawing flares in screen space
	double modelMat[16], projMat[16];
	GLint viewport[4];
	
	// transformation needed for rendering particles
	float billboardMat[16];
	
	// lifespans for smoke particles
	float smokeTime[SMOKETIMES];  // lifespans of consecutive smoke particles
	int whichSmoke[WHICHSMOKES];  // table to indicate which particles produce smoke
								  // smoke display lists
	unsigned int smokelist[5];
	
	// the world
	World* theWorld;
	
	// text output
	rsText* textwriter;
	
	
	int numRockets /*= 0*/;
	std::vector<flareData> lensFlares;
	unsigned int numFlares /*= 0*/;
	// Parameters edited in the dialog box
    int dMaxrockets;
    int dSmoke;
    int dExplosionsmoke;
    int dWind;
    int dAmbient;
    int dStardensity;
    int dFlare;
    int dMoonglow;
	int dSound;
    int kCamera;  // 0 = paused, 1 = autonomous, 2 = mouse control
    int dMoon;
    int dClouds;
    int dEarth;
    int dIllumination;
	int dFrameRateLimit;
	int kStatistics;
	bool kSlowMotion;
	// Commands given from keyboard
	int kFireworks /*= 1*/;
	int kNewCamera /*= 0*/;
	int userDefinedExplosion/* = -1*/;
	
    // Additional globals
    float frameTime;
	int first;
	
	std::vector<particle> particles;
	unsigned int last_particle/* = 0*/;
#define ZOOMROCKETINACTIVE 1000000000
	unsigned int zoomRocket/* = ZOOMROCKETINACTIVE*/;
} SkyrocketSaverSettings;


__private_extern__ void draw(SkyrocketSaverSettings * inSettings);

__private_extern__ void initSaver(int width,int height,SkyrocketSaverSettings * inSettings);

__private_extern__ void setDefaults(SkyrocketSaverSettings * inSettings);

__private_extern__ void cleanup(SkyrocketSaverSettings * inSettings);

// NZ: Some Windows-isms here:
enum
{
	WM_KEYDOWN,
	WM_MOUSEMOVE,
	WM_LBUTTONDOWN,
	WM_LBUTTONUP,
	WM_MBUTTONDOWN,
	WM_MBUTTONUP,
	WM_RBUTTONDOWN,
	WM_RBUTTONUP
};
typedef u_int32_t DWORD;
typedef u_int8_t BYTE;
typedef u_int16_t WORD;
typedef u_int32_t LONG;
#define MK_LBUTTON (1 << 1)  /* same as NSLeftMouseDownMask */
#define MK_MBUTTON (1 << 25) /* same as NSOtherMouseDownMask */
#define MK_RBUTTON (1 << 3)  /* same as NSRightMouseDownMask */

/*#define MAKEWORD(low, high) ((short)(((char)(low)) | (((short)((char)(high))) << 8)))
#define HIWORD(x) ((short)((long)(x) >> 16))
#define LOWORD(x) ((short)(x))*/
#define MAKEWORD(a, b)      ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a, b)      ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define LOWORD(l)           ((WORD)(l))
#define HIWORD(l)           ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
long ScreenSaverProc(unsigned int msg, unsigned int wpm, unsigned long lpm, SkyrocketSaverSettings *inSettings);

#endif
