#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "menu.h"
#include "pad.h"
#include "video.h"

void DrawHeading(void)
{
	// Draw a nice heading.
	cls();
	puts("csm-installer v1.5 by thepikachugamer");
	puts("Super basic menu (2)");
	// strikethrough
	printf(CONSOLE_ESC(9m) "%*s" CONSOLE_ESC(29m) "\n", gConsole.windowWidth, "");
}

void DrawFooter(int controls)
{
	int prevX = gConsole.cursorX;
	int prevY = gConsole.cursorY;
	{
		gConsole.cursorX = 1;
		gConsole.cursorY = gConsole.windowHeight - 3;
		if (controls) {
			gConsole.cursorY--;
			printf("  Controls:%*s\n", gConsole.windowWidth - 12, "");
			printf(CONSOLE_ESC(9m) "%*s" CONSOLE_ESC(29m) "\n", gConsole.windowWidth - 1, "");
			switch (controls) {
				case 1:
					printf("	(A) : Select                              \x12 Up/Down : Move cursor\n");
					printf("	[B] : Nothing                            Home/Start : Return to HBC");
					break;
				case 2:
					printf("	\x1d Left/Right : Change setting             \x12 Up/Down : Move cursor\n");
					printf("	         [B] : Main menu                 Home/Start : Main menu");
			}
		}
		else {
			printf(CONSOLE_ESC(9m) "%*s" CONSOLE_ESC(29m) "\n", gConsole.windowWidth - 1, "");
		}
	}
	gConsole.cursorX = prevX;
	gConsole.cursorY = prevY;
}

__attribute__((weak))
void deinitialize(void) { }

void MainMenu(int argc; MainMenuItem argv[argc], int argc)
{
	int x = 0, ystart = 0;

	DrawHeading();
	DrawFooter(1);

	ystart = gConsole.cursorY;
	while (true)
	{
		MainMenuItem* item = argv + x;

		gConsole.cursorX = 1;
		gConsole.cursorY = ystart;

		for (MainMenuItem* i = argv; i < argv + argc; i++)
		{
			if (i == item) printf("%s>>  %s" CONSOLE_RESET "\n", i->highlight_str ?: "", i->name);
			else printf("    %s\n", i->name);
		}

		switch (input_wait(0))
		{
			case INPUT_UP:
			{
				if (x-- == 0) x = argc - 1;
				break;
			}

			case INPUT_DOWN:
			{
				if (++x == argc) x = 0;
				break;
			}

			case INPUT_A:
			{
				if (!item->action) return;

				cls();
				if (item->heading) { DrawHeading(); DrawFooter(false); }
				item->action();

				if (item->pause) {
					puts("\nPress any button to continue...");
					input_wait(0);
				}

				cls();
				DrawHeading();
				DrawFooter(1);
				break;
			}

			// case INPUT_B:
			case INPUT_HOME:
			{
				// deinitialize();
				// exit(0);
				return;
			}
		}
	}
}

void SettingsMenu(int argc; SettingsItem argv[argc], int argc)
{
	int x = 0, ystart;

	DrawHeading();
	DrawFooter(2);

	ystart = gConsole.cursorY;
	while (true)
	{
		SettingsItem* selected = argv + x;
		int* selectedopt = selected->selected; // !!!?

		gConsole.cursorX = 1;
		gConsole.cursorY = ystart;

		for (SettingsItem* i = argv; i < argv + argc; i++)
		{
			if (i == selected)
				printf_clearln("	%-40s	%c %s %c\n", i->name, (*i->selected) ? '<':' ', i->options[*i->selected], (((*i->selected) + 1) < i->count) ? '>':' ');
			else
				printf_clearln("	%-40s	  %s  \n", i->name, i->options[*i->selected]);
		}

		switch (input_wait(0))
		{
			case INPUT_UP:
			{
				if (x-- == 0) x = argc - 1;
				break;
			}

			case INPUT_DOWN:
			{
				if (++x == argc) x = 0;
				break;
			}

			case INPUT_LEFT:
			{
				if (*selectedopt) (*selectedopt)--;
				break;
			}

			case INPUT_RIGHT:
			{
				if ((*selectedopt) + 1 < selected->count) (*selectedopt)++;
				break;
			}

			case INPUT_A:
			case INPUT_B:
			{
				return;
			}
		}
	}
}
