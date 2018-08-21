/*
Copyright (C) 2007-2008 Kristian Duske

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#import "AppController.h"
#import "ScreenInfo.h"
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#if defined(USE_SDL2)
#import <SDL2/SDL.h>
#else
#import <SDL/SDL.h>
#endif
#else
#import "SDL.h"
#endif
#import "SDLMain.h"

NSString *FQPrefCommandLineKey = @"CommandLine";
NSString *FQPrefFullscreenKey = @"Fullscreen";
NSString *FQPrefScreenModeKey = @"ScreenMode";

@implementation AppController

+(void) initialize {
    NSMutableDictionary *defaults = [NSMutableDictionary dictionary];
    
    [defaults setObject:@"" forKey:FQPrefCommandLineKey];
    [defaults setObject:[NSNumber numberWithBool:YES] forKey:FQPrefFullscreenKey];
    [defaults setObject:[NSNumber numberWithInt:0] forKey:FQPrefScreenModeKey];
    
    [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];
}

- (id)init {
    int i;
#ifndef USE_SDL2
    int j;
    int flags;
    int bpps[3] = {32, 24, 16};
    SDL_PixelFormat format;
    SDL_Rect **modes;
#endif
    ScreenInfo *info;

    self = [super init];
    if (!self)
        return nil;

    screenModes = [[NSMutableArray alloc] init];
    [screenModes addObject:@"Default or command line arguments"];

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
        return self;
    
#if defined(USE_SDL2)
    {
        const int sdlmodes = SDL_GetNumDisplayModes(0);
        for (i = 0; i < sdlmodes; i++)
        {
            SDL_DisplayMode mode;
            if (SDL_GetDisplayMode(0, i, &mode) == 0)
            {
                info = [[ScreenInfo alloc] initWithWidth:mode.w height:mode.h bpp:SDL_BITSPERPIXEL(mode.format)];
                [screenModes addObject:info];
                [info release];
            }
        }
    }
#else
    flags = SDL_OPENGL | SDL_FULLSCREEN;
    format.palette = NULL;
    
    for (i = 0; i < 3; i++) {
        format.BitsPerPixel = bpps[i];
        modes = SDL_ListModes(&format, flags);

        if (modes == (SDL_Rect **)0 || modes == (SDL_Rect **)-1)
            continue;

        for (j = 0; modes[j]; j++) {
            info = [[ScreenInfo alloc] initWithWidth:modes[j]->w height:modes[j]->h bpp:bpps[i]];
            [screenModes addObject:info];
            [info release];
        }
    }
#endif

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    
    arguments = [[QuakeArguments alloc] initWithArguments:gArgv + 1 count:gArgc - 1];
    return self;
}

- (NSArray *)screenModes {
    return screenModes;
}

- (void)awakeFromNib {
    if ([arguments count] > 0) {
        [paramTextField setStringValue:[arguments description]];
        if ([arguments argument:@"-window"] != nil)
            [fullscreenCheckBox setState:NSOffState];
    } else {
		NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        [paramTextField setStringValue:[defaults stringForKey:FQPrefCommandLineKey]];
        
        BOOL fullscreen = [defaults boolForKey:FQPrefFullscreenKey];
        [fullscreenCheckBox setState:fullscreen ? NSOnState : NSOffState];
        
        int screenModeIndex = [defaults integerForKey:FQPrefScreenModeKey];
        [screenModePopUp selectItemAtIndex:screenModeIndex];
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	if ([arguments argument:@"-nolauncher"] != nil) {
		[arguments removeArgument:@"-nolauncher"];
		[self launchQuake:self];
	} else {
        [launcherWindow center];
		[launcherWindow makeKeyAndOrderFront:self];
	}
}

- (IBAction)changeScreenMode:(id)sender {
    int index = [screenModePopUp indexOfSelectedItem];
    [fullscreenCheckBox setEnabled:index != 0];
}

- (IBAction)launchQuake:(id)sender {
    [arguments parseArguments:[paramTextField stringValue]];
    
    int index = [screenModePopUp indexOfSelectedItem];
    if (index > 0) {
        ScreenInfo *info = [screenModes objectAtIndex:index];
        
        int width = [info width];
        int height = [info height];
        int bpp = [info bpp];

        [arguments addArgument:@"-width" withValue:[NSString stringWithFormat:@"%d", width]];
        [arguments addArgument:@"-height" withValue:[NSString stringWithFormat:@"%d", height]];
        [arguments addArgument:@"-bpp" withValue:[NSString stringWithFormat:@"%d", bpp]];
    }
    
    [arguments removeArgument:@"-fullscreen"];
    [arguments removeArgument:@"-window"];
    BOOL fullscreen = [fullscreenCheckBox state] == NSOnState;
    if (fullscreen)
        [arguments addArgument:@"-fullscreen"];
    else
        [arguments addArgument:@"-window"];

    NSString *path = [NSString stringWithCString:gArgv[0] encoding:NSASCIIStringEncoding];
    
    int i;
    for (i = 0; i < 4; i++)
        path = [path stringByDeletingLastPathComponent];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager changeCurrentDirectoryPath:path];
    
    int argc = [arguments count] + 1;
    char *argv[argc];
    
    argv[0] = gArgv[0];
    [arguments setArguments:argv + 1];

    [launcherWindow close];

    // update the defaults
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:[paramTextField stringValue] forKey:FQPrefCommandLineKey];
    [defaults setObject:[NSNumber numberWithBool:[fullscreenCheckBox state] == NSOnState] forKey:FQPrefFullscreenKey];
    [defaults setObject:[NSNumber numberWithInt:index] forKey:FQPrefScreenModeKey];
    [defaults synchronize];

    int status = SDL_main (argc, argv);
    exit(status);
}

- (IBAction)cancel:(id)sender {
    exit(0);
}

- (void) dealloc {
    [screenModes release];
    [super dealloc];
}


@end
