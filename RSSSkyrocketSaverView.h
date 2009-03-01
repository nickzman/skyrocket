#import <AppKit/AppKit.h>
#import <ScreenSaver/ScreenSaver.h>
#import <OpenGL/gl.h>
#import <OpenGL/glu.h>

#define __MACOSXSCREENSAVER__
#include "Skyrocket.h"

@interface RSSSkyrocketSaverView : ScreenSaverView
{
	NSOpenGLView *_view;
    
    SkyrocketSaverSettings settings_;
    
    int mainScreenOnly_;
    
    BOOL isConfiguring_;
    BOOL preview_;
    BOOL mainScreen_;
	BOOL soundDisabled_;

    float times[10];
    int timeindex;
    
    unsigned long long tLastTime;

    IBOutlet id IBconfigureSheet_;
    IBOutlet id IBambientSlider_;
    IBOutlet id IBambientText_;
    IBOutlet id IBflareSlider_;
    IBOutlet id IBflareText_;
    IBOutlet id IBmaxRocketsStepper_;
    IBOutlet id IBmaxRocketsTextField_;
    IBOutlet id IBmoonGlowSlider_;
    IBOutlet id IBmoonGlowText_;
    IBOutlet id IBsmokeExplosionsSlider_;
    IBOutlet id IBsmokeExplosionsText_;
    IBOutlet id IBsmokeSlider_;
    IBOutlet id IBsmokeText_;
    IBOutlet id IBstarDensitySlider_;
    IBOutlet id IBstarDensityText_;
    IBOutlet id IBwindSlider_;
    IBOutlet id IBwindText_;
	IBOutlet id IBsoundSlider_;
	IBOutlet id IBsoundText_;
    IBOutlet id IBclouds_;
    IBOutlet id IBearth_;
    IBOutlet id IBillumination_;
    IBOutlet id IBmoon_;
    IBOutlet id IBcamera_;
	IBOutlet id IBslowMotion_;
    IBOutlet id IBmainScreen_;
    
    IBOutlet id IBversion_;
}

- (IBAction)closeSheet:(id)sender;
- (IBAction)reset:(id)sender;
- (IBAction)setAmbient:(id)sender;
- (IBAction)setExplosionsmoke:(id)sender;
- (IBAction)setFlare:(id)sender;
- (IBAction)setMaxrockets:(id)sender;
- (IBAction)setMoonglow:(id)sender;
- (IBAction)setSmoke:(id)sender;
- (IBAction)setStardensity:(id)sender;
- (IBAction)setWind:(id)sender;
- (IBAction)setSoundVolume:(id)sender;



- (void) readDefaults:(ScreenSaverDefaults *) inDefaults;
- (void) writeDefaults;

@end
