//
//  MacHelperFunctions.mm
//  Skyrocket
//
//  Created by Nick Zitzmann on 2/25/06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "MacHelperFunctions.h"
#import "RSSSkyrocketSaverView.h"

const char *PathForResourceOfType(const char *resourceName, const char *type)
{
	NSString *resourceNameString = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:resourceName length:strlen(resourceName)];
	NSString *typeString = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:type length:strlen(type)];
	NSString *path = [[NSBundle bundleForClass:[RSSSkyrocketSaverView class]] pathForResource:resourceNameString ofType:typeString];
	
	if (path)
		return [path fileSystemRepresentation];
	return NULL;
}
