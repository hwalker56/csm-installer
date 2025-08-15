#include "directory.h"

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "video.h"
#include "pad.h"
#include "fatMounter.h"

struct entry {
	char name[NAME_MAX];
	bool isdir;
};

static char* goBack(char* path) {
	if (strchr(path, '/') == strrchr(path, '/'))
		return NULL;

	*strrchr(path, '/') = '\x00';
	*(strrchr(path, '/') + 1) = '\x00';
	return path;
}

bool hasFileExtension(const char* name, const char* ext) {
	if (!(name = strrchr(name, '.')))
		return false;

	return !strcasecmp(name, ext);
}

static int entry_sort(const void* a_, const void* b_) {
	const struct entry *a = a_, *b = b_;

	if (a->isdir ^ b->isdir)
		return b->isdir - a->isdir;

	return strcasecmp(a->name, b->name);
}

static void PrintEntries(struct entry entries[], int start, int count, int max, int selected) {
	int i = 0;
	if (!count) {
		printf("	" CONSOLE_ESC(37;2m) "[Nothing.]" CONSOLE_RESET);
	}
	else {
		for (i = 0; i < MIN(max, count); i++) {
		printf("%2s	%s\n",
			(start + i) == selected? ">>" : "",
			entries[start + i].name);
		}
	}
}

static bool GetDirectoryEntries(const char* path, struct entry** entries, int* count, FileFilter filter) {
	if (!entries || !count || !path) return NULL;

	DIR* pdir;
	struct dirent* ent = NULL;
	struct entry* _entries = NULL;
	int cnt = 0;

	pdir = opendir(path);
	if (!pdir)
		return false;

	while ((ent = readdir(pdir))) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..") || !strncmp(ent->d_name, "._", 2) )
			continue;

		bool isdir = (ent->d_type == DT_DIR);
		if (!isdir && !filter(ent->d_name))
			continue;

		// If ptr is NULL, then the call is equivalent to malloc(size), for all values of size.
		_entries = reallocarray(*entries, cnt + 1, sizeof(struct entry));
		if (!_entries) {
			free(*entries);
			*entries = NULL;
			errno = ENOMEM;
			return false;
		}
		*entries = _entries;

		struct entry* entry = &_entries[cnt++];

		memset(entry, 0, sizeof(struct entry));
		strcpy(entry->name, ent->d_name);
		if ((entry->isdir = isdir))
			strcat(entry->name, "/");
	}

	qsort(_entries, cnt, sizeof(struct entry), entry_sort);

	*count = cnt;

	closedir(pdir);
	return true;
}

int SelectFileMenu(const char* header, const char* defaultFolder, FileFilter filter, char filepath[PATH_MAX]) {
	static char workpath[PATH_MAX];
	struct entry* entries = NULL;
	int cnt = 0, start = 0, index = 0, max = gConsole.windowHeight - 8;

	if (defaultFolder) {
		sprintf(workpath, "%s:%s/", GetActiveDeviceName(), defaultFolder);

		if (GetDirectoryEntries(workpath, &entries, &cnt, filter))
			goto a;
	}

	sprintf(workpath, "%s:/", GetActiveDeviceName());
	GetDirectoryEntries(workpath, &entries, &cnt, filter);

a:
	for(;;) {
		cls();
		if (!entries)
			return -1;

		struct entry* entry = &entries[index];

		const char* workpath_ptr = (strlen(workpath) > 30) ? workpath + strlen(workpath) - 30 : workpath;
		printf(
			"%s\n"
			"Current directory: [%s] - %i-%i of %i total\n"
			CONSOLE_ESC(9m) "%*s" CONSOLE_ESC(29m) "\n",
		header ?: "Select a file.",
		workpath_ptr, start + 1, start + MIN(max, cnt - start), cnt,
		gConsole.windowWidth - 1, "");

		PrintEntries(entries, start, cnt, max, index);

		gConsole.cursorX = 1;
		gConsole.cursorY = gConsole.windowHeight - 4;
		printf(
			"  Controls:%*s\n"
			CONSOLE_ESC(9m) "%*s\n" CONSOLE_ESC(29m)
			"	(A) : %-35s  \x12 Up/Down : Move cursor\n"
			"	[B] : %-35s Home/Start : Main menu",

			gConsole.windowWidth - 12, "",
			gConsole.windowWidth - 1, "",
			(!cnt) ? "Exit directory" : ((entry->isdir) ? "Enter directory" : "Install theme file"),
			(strchr(workpath, '/') == strrchr(workpath, '/')) ? "Main menu" : "Exit directory"
		);

		u32 buttons = input_wait(0);
		if (buttons & INPUT_DOWN) {
			if (index >= (cnt - 1))
				start = index = 0;

			else if ((++index - start) >= max)
				start++;
		}
		else if (buttons & INPUT_UP) {
			if (index <= 0) {
				index = cnt - 1;
				if (index >= max)
					start = 1 + index - max;
			}

			else if (--index < start)
				start--;

		}
		else if (buttons & INPUT_A) {
			if (!cnt) {
				goBack(workpath);
				GetDirectoryEntries(workpath, &entries, &cnt, filter);
				index = start = 0;
			}
			else if (entry->isdir) {
				strcat(workpath, entry->name);
				// strcat(workpath, "/");
				GetDirectoryEntries(workpath, &entries, &cnt, filter);
				index = start = 0;
			}
			else {
				strcpy(filepath, workpath);
				strcat(filepath, entry->name);
				free(entries);
				return 0;
			}
		}
		else if (buttons & INPUT_B) {
			if (!goBack(workpath)) {
				free(entries);
				entries = NULL;
				errno = ECANCELED;
				return -1;
			}

			GetDirectoryEntries(workpath, &entries, &cnt, filter);
			index = start = 0;
		}
		else if (buttons & INPUT_START) {
			free(entries);
			entries = NULL;
			errno = ECANCELED;
			return -1;
		}
	}

	return 0;
}
