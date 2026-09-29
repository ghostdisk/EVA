#include <EVA/PAL/PAL.hpp>
#import <AppKit/AppKit.h>

namespace EVA::PAL
{
void EmitEvent(Event event);
}

@interface EVAApplicationDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation EVAApplicationDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
	EVA::PAL::EmitEvent({ .type = EVA::PAL::EventType::QUIT_REQUESTED });
	// AppMain owns shutdown, including GPU and window cleanup.
	return NSTerminateCancel;
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
	EVA::PAL::EmitEvent({ .type = EVA::PAL::EventType::CLOSE_REQUESTED });
	// Keep the native window alive until the engine accepts the request.
	return NO;
}

@end

namespace EVA::PAL
{

static EVAApplicationDelegate* application_delegate = nil;

void InitBackend()
{
	@autoreleasepool
	{
		[NSApplication sharedApplication];
		[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
		application_delegate = [[EVAApplicationDelegate alloc] init];
		[NSApp setDelegate:application_delegate];

		NSString* app_name = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
		if (!app_name)
			app_name = [[NSProcessInfo processInfo] processName];

		NSMenu* main_menu = [[NSMenu alloc] init];
		NSMenuItem* app_item = [[NSMenuItem alloc] init];
		NSMenu* app_menu = [[NSMenu alloc] initWithTitle:app_name];
		[app_menu addItemWithTitle:[@"Quit " stringByAppendingString:app_name]
			action:@selector(terminate:) keyEquivalent:@"q"];
		[app_item setSubmenu:app_menu];
		[main_menu addItem:app_item];

		NSMenuItem* window_item = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
		NSMenu* window_menu = [[NSMenu alloc] initWithTitle:@"Window"];
		[window_menu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
		[window_menu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];
		[window_item setSubmenu:window_menu];
		[main_menu addItem:window_item];
		[NSApp setMainMenu:main_menu];
		[NSApp setWindowsMenu:window_menu];
		[NSApp finishLaunching];
	}
}

void InitWindow(Window* window, const WindowInitOptions& options)
{
	@autoreleasepool
	{
		NSRect content_rect = NSMakeRect(0, 0,
			options.width > 0 ? options.width : 800,
			options.height > 0 ? options.height : 600);
		NSWindow* native_window = [[NSWindow alloc]
			initWithContentRect:content_rect
			styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
				NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
			backing:NSBackingStoreBuffered defer:NO];
		[native_window setReleasedWhenClosed:NO];
		[native_window setDelegate:application_delegate];
		NSString* title = options.name ? [NSString stringWithUTF8String:options.name] : @"";
		[native_window setTitle:title ? title : @""];
		[native_window setBackgroundColor:[NSColor blackColor]];
		[native_window center];
		[native_window makeKeyAndOrderFront:nil];
		if (@available(macOS 14.0, *))
			[NSApp activate];
		else
			[NSApp activateIgnoringOtherApps:YES];

		// PAL owns the NSWindow until DeinitWindow; GPU can borrow this handle.
		window->native_handle = (__bridge_retained void*)native_window;
	}
}

void DeinitWindow(Window* window)
{
	@autoreleasepool
	{
		NSWindow* native_window = (__bridge_transfer NSWindow*)window->native_handle;
		window->native_handle = nullptr;
		[native_window setDelegate:nil];
		[native_window close];
	}
}

void PollBackend()
{
	@autoreleasepool
	{
		NSEvent* event = nil;
		while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
			untilDate:[NSDate distantPast] inMode:NSDefaultRunLoopMode dequeue:YES]))
		{
			[NSApp sendEvent:event];
		}
		[NSApp updateWindows];
	}
}

}
