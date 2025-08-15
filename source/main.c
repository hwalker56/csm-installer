#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <gccore.h>
#include <ogc/es.h>
#include <ogc/cache.h>
#include <wiiuse/wpad.h>
// #include <libpatcher.h>
#include "superuser.h"

#include "video.h"
#include "common.h"
#include "pad.h"
#include "fs.h"
#include "fatMounter.h"
#include "directory.h"
#include "menu.h"
#include "crypto.h"
#include "sysmenu.h"
#include "theme.h"
#include "network.h"

extern void __exception_setreload(int);

static int ww43dbmode = 0, brickprotection = 0;

bool isCSMfile(const char* name) {
	return hasFileExtension(name, ".csm") || hasFileExtension(name, ".app");
}

int SelectTheme() {
	int ret;
	char file[PATH_MAX] = {};
	size_t fsize = 0;
	void* buffer = NULL;

	ret = SelectFileMenu("Select a .csm or .app file.", "/themes", isCSMfile, file);
	cls();

	if (ret) {
		perror("SelectFileMenu failed");
		return ret;
	}

	DrawHeading();

	printf("%s\n\n", file);

	if (FAT_GetFileSize(file, &fsize) < 0) {
		perror("FAT_GetFileSize failed");
		return -errno;
	}

	printf("File size: %.2fMB\n\n", fsize / (float)(1 << 20));

	printf("Press +/START to install.\n"
			"Press any other button to cancel.\n\n");

	if (!(input_wait(0) & INPUT_START))
		return -ECANCELED;

	buffer = memalign32(fsize);
	if (!buffer) {
		printf("No memory..? (failed to allocate %u bytes)\n", fsize);
		return -ENOMEM;
	}

	ret = FAT_Read(file, buffer, fsize, progressbar);
	if (ret < 0) {
		perror("FAT_Read failed");
		goto finish;
	}

	ret = InstallTheme(buffer, fsize, ww43dbmode == 0);

finish:
	free(buffer);
	return ret;
}

int DownloadOriginal(void) {
	return DownloadOriginalTheme(false);
}

static SettingsItem settings[] = {
	{
		.name = "43DB fix for WC24 channels (vWii)",
		.options = (const char*[]){ "Enabled", "Disabled" },
		.count = 2,
		.selected = &ww43dbmode
	},

	{
		.name = "Brick Protection",
		.options = (const char*[]){ "Enabled", "Active", "Yes", "On", "Offn't" },
		.count = 5,
		.selected = &brickprotection
	}
};

int options(void)
{
	SettingsMenu(settings, 2);
	return 0;
}

int ReloadDevices(void)
{
	FATUnmount();
	if (FATMount())
		FATSelectDefault();
	else
		input_wait(0);

	return 0;
}

void deinitialize(void)
{
	network_deinit();
	ISFS_Deinitialize();
	FATUnmount();
	input_shutdown();
}

[[noreturn]]
int HBC(void)
{
	deinitialize();
	exit(0);
}

[[noreturn]]
int WiiMenu(void)
{
	deinitialize();

	if (sysmenu->hasPriiloader) {
		// Magic word 'Pune': force boot System Menu
		uint32_t* const priiloaderMagicWord = (uint32_t*)0x817FEFF0; // I like this one more because it's actually aligned on a 4 byte boundary
		*priiloaderMagicWord = 0x50756E65;
		DCFlushRange(priiloaderMagicWord, sizeof(uint32_t));
	}

	int ret = WII_LaunchTitle(0x100000002LL);
	__builtin_unreachable();

	printf("WII_LaunchTitle returned %i\n", ret);
	usleep(3000000);
	exit(ret);
}

static MainMenuItem items[] = {
	{
		.name = "Install a theme\n",
		.action = SelectTheme,
		.pause = true
	},


	{
		.name = "Save a backup of the current theme",
		.action = SaveCurrentTheme,
		.heading = true,
		.pause = true,
	},

	{
		.name = "Download original Wii theme (Base theme)",
		.action = DownloadOriginal,
		.heading = true,
		.pause = true
	},

	{
		.name = "Apply 43DB fix to current theme (vWii)\n",
		.action = PatchThemeInPlace,
		.heading = true,
		.pause = true
	},


	{
		.name = "Reload devices",
		.action = ReloadDevices,
		.heading = true
	},

	{
		.name = "Settings\n",
		.action = options
	},


	{
		.name = "Return to the Homebrew Channel",
		.action = HBC
	},

	{
		.name = "Return to Wii System Menu",
		.action = WiiMenu,
		.pause = true
	},
};
// I've forgotten to update the number in every instance I've had to, so it's time to do this
#define NBR_ITEMS (sizeof(items) / sizeof(MainMenuItem))

[[gnu::const]]
bool is_dolphin(void) {
	int fd = IOS_Open("/dev/dolphin", 0);
	if (fd >= 0) {
		IOS_Close(fd);
		return true;
	}

	fd = IOS_Open("/dev/aes", 0);
	if (fd >= 0) {
		IOS_Close(fd);
	}

	return (fd == IPC_ENOENT);
}

int main() {
	__exception_setreload(10);

	DrawHeading();
	DrawFooter(false);

	puts("Loading...");

	if (!is_dolphin() && !get_root_access()) {
		puts("Failed to obtain root access...\n\n"

			 "Exiting in 5 seconds..."
		);
		sleep(5);
		return -1;
	}

	input_init();
	ISFS_Initialize();

	SetupCommonKeys();
	if (sysmenu_process() < 0)
		goto waitexit;

	if (!FATMount()) {
		puts("Unable to mount a storage device...");
		goto waitexit;
	}

	DownloadOriginalTheme(true);

	if (!is_dolphin() && !sysmenu->hasPriiloader) {
		puts(CONSOLE_BG_RED "Please install Priiloader!!!!" CONSOLE_RESET);
		sleep(2);

		goto waitexit;

	}

	MainMenu(items, NBR_ITEMS);

exit:
	deinitialize();
	return 0;

waitexit:
	puts("Press any button to exit.");
	input_wait(0);
	goto exit;
}
