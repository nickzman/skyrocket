// Mac OS X Code
// 
// First version: 07/07/02 Stéphane Sudre

#import "RSSSkyrocketSaverView.h"
#include "Skyrocket.h"
#include "resource.h"
#include <sys/time.h>
#import <OpenGL/OpenGL.h>

@implementation RSSSkyrocketSaverView

- (id)initWithFrame:(NSRect)frameRect isPreview:(BOOL) preview
{
    NSString *identifier = [[NSBundle bundleForClass:[self class]] bundleIdentifier];
    ScreenSaverDefaults *defaults = [ScreenSaverDefaults defaultsForModuleWithName:identifier];
    id tObject;
    
    self = [super initWithFrame:frameRect isPreview:preview];
        
    preview_=preview;
    
    isConfiguring_=NO;
    
    if (preview_==YES)
    {
        mainScreen_=YES;
    }
    else
    {
        mainScreen_= (frameRect.origin.x==0 && frameRect.origin.y==0) ? YES : NO;
    }
    
    mainScreenOnly_=int([defaults integerForKey:@"MainScreen Only"]);
    
    if (self)
    {
#ifdef __ppc__
		long osVersion;
		
		Gestalt(gestaltSystemVersion, &osVersion);
		soundDisabled_ = (osVersion < 0x1050);
#else
		soundDisabled_ = NO;
#endif
        if (mainScreenOnly_!=NSOnState || mainScreen_==YES)
        {
            NSOpenGLPixelFormatAttribute attribs[] = 
            {
				NSOpenGLPFAAccelerated, (NSOpenGLPixelFormatAttribute)YES,
				NSOpenGLPFADoubleBuffer, (NSOpenGLPixelFormatAttribute)YES,
				NSOpenGLPFAMinimumPolicy, (NSOpenGLPixelFormatAttribute)YES,
				(NSOpenGLPixelFormatAttribute)0
            };
            
            NSOpenGLPixelFormat *format = [[[NSOpenGLPixelFormat alloc] initWithAttributes:attribs] autorelease];
            
            if (format!=nil)
            {
                _view = [[[NSOpenGLView alloc] initWithFrame:NSZeroRect pixelFormat:format] autorelease];
                [self addSubview:_view];
            
                settings_.frameTime=0;
                
				setDefaults(&settings_);
                tObject=[defaults objectForKey:@"WindSpeed"];
                if (tObject)
                {
                    [self readDefaults:defaults];
                }
                
                [self setAnimationTimeInterval:0.03];
            }
        }
    }
    
    return self;
}

- (void) drawRect:(NSRect) inFrame
{
	[[NSColor blackColor] set];
            
    NSRectFill(inFrame);
    
    if (_view==nil)
    {    
        if (mainScreenOnly_!=NSOnState || mainScreen_==YES)
        {
            NSRect tFrame=[self frame];
            NSRect tStringFrame;
            NSDictionary * tAttributes;
            NSString * tString;
            NSMutableParagraphStyle * tParagraphStyle;
            
            tParagraphStyle=[[NSParagraphStyle defaultParagraphStyle] mutableCopy];
            [tParagraphStyle setAlignment:NSCenterTextAlignment];
            
            tAttributes = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont systemFontOfSize:[NSFont systemFontSize]],NSFontAttributeName,[NSColor whiteColor],NSForegroundColorAttributeName,tParagraphStyle,NSParagraphStyleAttributeName,nil];
            
            [tParagraphStyle release];
            
            tString=NSLocalizedStringFromTableInBundle(@"Minimum OpenGL requirements\rfor this Screen Effect\rnot available\ron your graphic card.",@"Localizable",[NSBundle bundleForClass:[self class]],@"No comment");
            
            tStringFrame.origin=NSZeroPoint;
            tStringFrame.size=[tString sizeWithAttributes:tAttributes];
            
            tStringFrame=SSCenteredRectInRect(tStringFrame,tFrame);
            
            [tString drawInRect:tStringFrame withAttributes:tAttributes];
            
            return;
        }
    }
}

- (void) setFrameSize:(NSSize)newSize
{
	[super setFrameSize:newSize];
    
    if (_view!=nil)
    {
        [_view setFrameSize:newSize];
    }
}

- (void)animateOneFrame
{
    if (isConfiguring_==NO && _view!=nil)
    {
        if (mainScreenOnly_!=NSOnState || mainScreen_==YES)
        {
            struct timeval tTime;
            unsigned long long tCurentTime;
            
            [[_view openGLContext] makeCurrentContext];
            
            gettimeofday(&tTime, NULL);
            
            tCurentTime=(tTime.tv_sec*1000+tTime.tv_usec/1000);
            
            if(tCurentTime >= tLastTime)
                times[timeindex] = float(tCurentTime - tLastTime) * 0.001f;
            else  // else use elapsedTime from last frame
                times[timeindex] = settings_.frameTime;
            
            
            settings_.frameTime = 0.1f * (times[0] + times[1] + times[2] + times[3] + times[4]
            + times[5] + times[6] + times[7] + times[8] + times[9]);
        
            timeindex ++;
            if(timeindex >= 10)
                timeindex = 0;

            tLastTime=tCurentTime;
            
            draw(&settings_);
            
            [[_view openGLContext] flushBuffer];
        }
    }
}

- (void)startAnimation
{
    [super startAnimation];
    
    if (isConfiguring_==NO && _view!=nil)
    {
        if (mainScreenOnly_!=NSOnState || mainScreen_==YES)
        {
            NSSize tSize;
            struct timeval tTime;
            int i;
			GLint interval = 1;
            
            [self lockFocus];
            [[_view openGLContext] makeCurrentContext];
            
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glFlush();
            CGLSetParameter(CGLGetCurrentContext(), kCGLCPSwapInterval, &interval);	// don't allow screen tearing
            [[_view openGLContext] flushBuffer];
            
            tSize=[_view frame].size;
            
            //cleanSettings(&settings_);
			
            initSaver((int) tSize.width,(int) tSize.height,&settings_);

            for(i=0;i<10;i++)
            {
                times[i]=0.03f;
            }

            timeindex = 0;

            settings_.frameTime=0;

            [self unlockFocus];

            gettimeofday(&tTime, NULL);

            tLastTime=(tTime.tv_sec*1000+tTime.tv_usec/1000);
        }
    }
}

- (void)stopAnimation
{
    [super stopAnimation];
    
    if (_view!=nil)
    {
        if (mainScreenOnly_!=NSOnState || mainScreen_==YES)
        {
            [[_view openGLContext] makeCurrentContext];
            
            //cleanSettings(&settings_);
			cleanup(&settings_);
			
            settings_.frameTime=0;
        }
    }
}

- (BOOL) hasConfigureSheet
{
    return (_view!=nil);
}

- (void) readDefaults:(ScreenSaverDefaults *) inDefaults
{
    // jes: looks like I can make these what I want them to be as long as I'm 
    //      consistent throughout file.  I think this is the key for the prefs 
    //      file, so... make it user readable

    settings_.dMaxrockets=int([inDefaults integerForKey:@"MaxRockets"]);
    
    settings_.dSmoke=int([inDefaults integerForKey:@"SmokeLifespan"]);
    
    settings_.dExplosionsmoke=int([inDefaults integerForKey:@"SmokePerExplosion"]);
    
    settings_.dWind=int([inDefaults integerForKey:@"WindSpeed"]);
    
    settings_.dAmbient=int([inDefaults integerForKey:@"AmbientLight"]);
    
    settings_.dStardensity=int([inDefaults integerForKey:@"StarDensity"]);
    
    settings_.dFlare=int([inDefaults integerForKey:@"LensFlareBrightness"]);
    
    settings_.dMoonglow=int([inDefaults integerForKey:@"MoonGlowBrightness"]);
	if (soundDisabled_ == NO)
		settings_.dSound = int([inDefaults integerForKey:@"SoundVolume"]);
    settings_.kCamera=int([inDefaults integerForKey:@"CameraMovement"]);
	settings_.kSlowMotion = [inDefaults boolForKey:@"SlowMotion"];
    
    settings_.dMoon=int([inDefaults integerForKey:@"DrawMoon"]);
    
    settings_.dEarth=int([inDefaults integerForKey:@"DrawEarth"]);
    
    settings_.dClouds=int([inDefaults integerForKey:@"DrawClouds"]);
    
    settings_.dIllumination=int([inDefaults integerForKey:@"Illumination"]);

    mainScreenOnly_=int([inDefaults integerForKey:@"MainScreen Only"]);
}

- (void) writeDefaults
{
    NSString *identifier = [[NSBundle bundleForClass:[self class]] bundleIdentifier];
    ScreenSaverDefaults *inDefaults = [ScreenSaverDefaults defaultsForModuleWithName:identifier];
    
    mainScreenOnly_=int([IBmainScreen_ state]);
    
    settings_.dClouds=int([IBclouds_ state]);
    settings_.dEarth=int([IBearth_ state]);
    settings_.dMoon=int([IBmoon_ state]);
    settings_.dIllumination=int([IBillumination_ state]);
    settings_.kCamera=int([IBcamera_ state]);
	settings_.kSlowMotion = ([IBslowMotion_ state] == NSOnState ? true : false);
    
    [inDefaults setInteger:settings_.dMaxrockets forKey:@"MaxRockets"];
    
    [inDefaults setInteger:settings_.dSmoke forKey:@"SmokeLifespan"];
    
    [inDefaults setInteger:settings_.dExplosionsmoke forKey:@"SmokePerExplosion"];
    
    [inDefaults setInteger:settings_.dWind forKey:@"WindSpeed"];
    
    [inDefaults setInteger:settings_.dAmbient forKey:@"AmbientLight"];
    
    [inDefaults setInteger:settings_.dStardensity forKey:@"StarDensity"];
    
    [inDefaults setInteger:settings_.dFlare forKey:@"LensFlareBrightness"];
    
    [inDefaults setInteger:settings_.dMoonglow forKey:@"MoonGlowBrightness"];
    [inDefaults setInteger:settings_.dSound forKey:@"SoundVolume"];
    [inDefaults setInteger:settings_.kCamera forKey:@"CameraMovement"];
	[inDefaults setBool:settings_.kSlowMotion forKey:@"SlowMotion"];
    
    [inDefaults setInteger:settings_.dMoon forKey:@"DrawMoon"];
    
    [inDefaults setInteger:settings_.dEarth forKey:@"DrawEarth"];
    
    [inDefaults setInteger:settings_.dClouds forKey:@"DrawClouds"];
    
    [inDefaults setInteger:settings_.dIllumination forKey:@"Illumination"];
    
    [inDefaults setInteger:mainScreenOnly_ forKey:@"MainScreen Only"];
    
    [inDefaults  synchronize];
}

- (void) setDialogValue
{
    [IBmaxRocketsStepper_ setIntValue:settings_.dMaxrockets];
    [IBmaxRocketsTextField_ setIntValue:settings_.dMaxrockets];
    
    [IBambientSlider_ setIntValue:settings_.dAmbient];
    [IBambientText_ setIntValue:settings_.dAmbient];
    
    [IBflareSlider_ setIntValue:settings_.dFlare];
    [IBflareText_ setIntValue:settings_.dFlare];
    
    [IBmoonGlowSlider_ setIntValue:settings_.dMoonglow];
    [IBmoonGlowText_ setIntValue:settings_.dMoonglow];
    
    [IBsmokeExplosionsSlider_ setIntValue:settings_.dExplosionsmoke];
    [IBsmokeExplosionsText_ setIntValue:settings_.dExplosionsmoke];
    
    [IBsmokeSlider_ setIntValue:settings_.dSmoke];
    [IBsmokeText_ setIntValue:settings_.dSmoke];
    
    [IBstarDensitySlider_ setIntValue:settings_.dStardensity];
    [IBstarDensityText_ setIntValue:settings_.dStardensity];
    
    [IBwindSlider_ setIntValue:settings_.dWind];
    [IBwindText_ setIntValue:settings_.dWind];
	
	if (soundDisabled_ == NO)
	{
		[IBsoundSlider_ setIntValue:settings_.dSound];
		[IBsoundText_ setIntValue:settings_.dSound];
	}
	else
	{
		[IBsoundSlider_ setEnabled:NO];
		[IBsoundText_ setIntValue:0];
	}
    
    [IBmainScreen_ setState:mainScreenOnly_];
    [IBearth_ setState:settings_.dEarth];
    [IBmoon_ setState:settings_.dMoon];
    [IBillumination_ setState:settings_.dIllumination];
    [IBcamera_ setState:settings_.kCamera];
	[IBslowMotion_ setState:(settings_.kSlowMotion ? NSOnState : NSOffState)];
    [IBclouds_ setState:settings_.dClouds];
}

- (NSWindow*)configureSheet
{
    isConfiguring_=YES;
    
    if (IBconfigureSheet_ == nil)
    {
        [NSBundle loadNibNamed:@"ConfigureSheet" owner:self];
        
        [IBversion_ setStringValue:[[[NSBundle bundleForClass:[self class]] infoDictionary] objectForKey:@"CFBundleVersion"]];
        
        [self setDialogValue];
    }
    
    return IBconfigureSheet_;
}

- (IBAction)closeSheet:(id)sender
{
    [self writeDefaults];
    
    isConfiguring_=NO;
    
    if ([self isAnimating]==YES)
    {
        [self stopAnimation];
        [self startAnimation];
    }
    
    [NSApp endSheet:IBconfigureSheet_];
}


- (IBAction)reset:(id)sender
{
    setDefaults(&settings_);
    
    [self setDialogValue];
}

- (IBAction)setAmbient:(id)sender
{
    settings_.dAmbient=[sender intValue];
    
    [IBambientText_ setIntValue:settings_.dAmbient];
}

- (IBAction)setExplosionsmoke:(id)sender
{
    settings_.dExplosionsmoke=[sender intValue];
    
    [IBsmokeExplosionsText_ setIntValue:settings_.dExplosionsmoke];
}

- (IBAction)setMaxrockets:(id)sender
{
    settings_.dMaxrockets=[sender intValue];
    
    [IBmaxRocketsTextField_ setIntValue:settings_.dMaxrockets];
}

- (IBAction)setWind:(id)sender
{
    settings_.dWind=[sender intValue];
    
    [IBwindText_ setIntValue:settings_.dWind];
}

- (IBAction)setSmoke:(id)sender
{
    settings_.dSmoke=[sender intValue];
    
    [IBsmokeText_ setIntValue:settings_.dSmoke];
}

- (IBAction)setStardensity:(id)sender
{
    settings_.dStardensity=[sender intValue];
    
    [IBstarDensityText_ setIntValue:settings_.dStardensity];
}

- (IBAction)setFlare:(id)sender
{
    settings_.dFlare=[sender intValue];
    
    [IBflareText_ setIntValue:settings_.dFlare];
}

- (IBAction)setMoonglow:(id)sender
{
    settings_.dMoonglow=[sender intValue];
    
    [IBmoonGlowText_ setIntValue:settings_.dMoonglow];
}

- (IBAction)setSoundVolume:(id)sender
{
	settings_.dSound = [sender intValue];
	
	[IBsoundText_ setIntValue:settings_.dSound];
}


- (void)keyDown:(NSEvent *)anEvent
{
	if (ScreenSaverProc(WM_KEYDOWN, [[anEvent characters] characterAtIndex:0], 0, &settings_) != 0)
		[super keyDown:anEvent];
	
	[[self window] setAcceptsMouseMovedEvents:(settings_.kCamera == 2)];
}


- (void)mouseDown:(NSEvent *)anEvent
{
	NSPoint location = [anEvent locationInWindow];
	WORD locationx = WORD(location.x);
	WORD locationy = WORD([self frame].size.height-location.y);
	
	if (ScreenSaverProc(WM_LBUTTONDOWN, unsigned(NSEventMaskFromType([anEvent type])), MAKELONG(locationx, locationy), &settings_) != 0)
		[super mouseDown:anEvent];
}


- (void)mouseDragged:(NSEvent *)anEvent
{
	NSPoint location = [anEvent locationInWindow];
	WORD locationx = WORD(location.x);
	WORD locationy = WORD([self frame].size.height-location.y);
	
	if (ScreenSaverProc(WM_LBUTTONDOWN, MK_LBUTTON, MAKELONG(locationx, locationy), &settings_) != 0)
		[super mouseDragged:anEvent];
}


- (void)mouseMoved:(NSEvent *)anEvent
{
	NSPoint location = [anEvent locationInWindow];
	WORD locationx = WORD(location.x);
	WORD locationy = WORD([self frame].size.height-location.y);
	
	if (ScreenSaverProc(WM_MOUSEMOVE, unsigned(NSEventMaskFromType([anEvent type])), MAKELONG(locationx, locationy), &settings_) != 0)
		[super mouseMoved:anEvent];
}


- (void)mouseUp:(NSEvent *)anEvent
{
	NSPoint location = [anEvent locationInWindow];
	WORD locationx = WORD(location.x);
	WORD locationy = WORD([self frame].size.height-location.y);
	
	if (ScreenSaverProc(WM_LBUTTONUP, unsigned(NSEventMaskFromType([anEvent type])), MAKELONG(locationx, locationy), &settings_) != 0)
		[super mouseUp:anEvent];
}


- (void)rightMouseDown:(NSEvent *)anEvent
{
	NSPoint location = [anEvent locationInWindow];
	WORD locationx = WORD(location.x);
	WORD locationy = WORD([self frame].size.height-location.y);
	
	if (ScreenSaverProc(WM_RBUTTONDOWN, unsigned(NSEventMaskFromType([anEvent type])), MAKELONG(locationx, locationy), &settings_) != 0)
		[super rightMouseDown:anEvent];
}


- (void)rightMouseDragged:(NSEvent *)anEvent
{
	NSPoint location = [anEvent locationInWindow];
	WORD locationx = WORD(location.x);
	WORD locationy = WORD([self frame].size.height-location.y);
	
	if (ScreenSaverProc(WM_RBUTTONDOWN, MK_RBUTTON, MAKELONG(locationx, locationy), &settings_) != 0)
		[super rightMouseDragged:anEvent];
}


- (void)rightMouseUp:(NSEvent *)anEvent
{
	NSPoint location = [anEvent locationInWindow];
	WORD locationx = WORD(location.x);
	WORD locationy = WORD([self frame].size.height-location.y);
	
	if (ScreenSaverProc(WM_RBUTTONUP, unsigned(NSEventMaskFromType([anEvent type])), MAKELONG(locationx, locationy), &settings_) != 0)
		[super rightMouseUp:anEvent];
}

@end


@implementation NSOpenGLView (HacksRUs)

- (void)rightMouseDown:(NSEvent *)anEvent
{
	[[self nextResponder] rightMouseDown:anEvent];
}

@end
