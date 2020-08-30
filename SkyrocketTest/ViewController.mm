//
//  ViewController.mm
//  SkyrocketTest
//
//  Created by Nick Zitzmann on 8/24/20.
//

#import "ViewController.h"
#import "RSSSkyrocketSaverView.h"

@interface ViewController ()
@property(nonatomic,strong) RSSSkyrocketSaverView *saverView;
@end

@implementation ViewController

- (void)viewDidAppear
{
	self.saverView = [[RSSSkyrocketSaverView alloc] initWithFrame:CGRectMake(0.0, 0.0, self.view.frame.size.width, self.view.frame.size.height) isPreview:NO];
	
	self.saverView.translatesAutoresizingMaskIntoConstraints = NO;
	[self.view addSubview:self.saverView];
	[self.view.leadingAnchor constraintEqualToAnchor:self.saverView.leadingAnchor].active = YES;
	[self.view.trailingAnchor constraintEqualToAnchor:self.saverView.trailingAnchor].active = YES;
	[self.view.topAnchor constraintEqualToAnchor:self.saverView.topAnchor].active = YES;
	[self.view.bottomAnchor constraintEqualToAnchor:self.saverView.bottomAnchor].active = YES;
	[self.saverView startAnimation];
}


- (IBAction)preferencesAction:(id)sender
{
	NSWindow *configureSheet = self.saverView.configureSheet;
	
	if (configureSheet)
		[self.view.window beginSheet:configureSheet completionHandler:NULL];
}
@end
