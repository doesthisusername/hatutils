#include <stdio.h>
#include <Windows.h>
#include <tlhelp32.h>

#define HAT_WINDOW L"LaunchUnrealUWindowsClient"
#define HAT_TITLE L"A Hat in Time (64-bit, Final Release Shipping PC, DX9, Modding)"
#define HAT_EXE_NAME L"HatinTimeGame.exe"

#define PE_SECTION_PTR (0x3C)
#define PE_TS_OFFSET (0x08)

enum {
	DLC15_8061143192026666389 = 1537061434, // dw patch
	DLC21_242922671485761424 = 1557549916, // any% patch
	DLC231_7770543545116491859 = 1561041656, // tas patch
	DLC232_5506509173732835905 = 1565114742, // 110% patch
	VER_MAX = 4
};

struct hotkey {
	unsigned int vk;
	float duration;
};

HWND window;
HANDLE process;
HANDLE thread;
DWORD pid;
DWORD tid;

BYTE* hat_address = NULL;
unsigned int pe_ts;
unsigned int i_am_testing = 0;

BYTE* fps_address = NULL;
char* fps_path = NULL;
char* fps_paths[VER_MAX] = {
	"0x11C27E0, 0x710",
	"0x11BC360, 0x710",
	"0x11F6F10, 0x710",
	"0x11F9FE0, 0x710"
};

unsigned int get_pe_ts(BYTE* base) {
	unsigned int tmp;
	ReadProcessMemory(process, base + PE_SECTION_PTR, &tmp, sizeof(tmp), NULL);
	ReadProcessMemory(process, base + tmp + PE_TS_OFFSET, &tmp, sizeof(tmp), NULL);

	return tmp;
}

BYTE* resolve_ptr(HANDLE process, void* address) {
	BYTE* resolved;
	ReadProcessMemory(process, address, &resolved, 8, NULL);
	return resolved;
}

void* resolve_ptr_path(HANDLE process, BYTE* base, const char* path) {
	char* next;
	unsigned long long offset;

	if(!process || !base) {
		return NULL;
	}

	unsigned long long resolved = (unsigned long long)base + strtoull(path, &next, 0);

	while(path + strlen(path) > next) {
		offset = strtoull(next + 1, &next, 0);
		ReadProcessMemory(process, (void*)resolved, &resolved, 8, NULL);
		resolved += offset;
	}

	return (void*)resolved;
}

int is_hat_open() {
	if(FindWindow(HAT_WINDOW, NULL) != NULL) {
		return 1;
	}
	return 0;
}

int init() {
	if(!is_hat_open()) {
		return 0;
	}

	// find pid
	window = FindWindow(HAT_WINDOW, NULL);
	tid = GetWindowThreadProcessId(window, &pid);

	// get process handle passing in the pid
	process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
	if(process == NULL) {
		printf("Failed to open the game process, press any key to exit.\n");
		getchar();
		return 0;
	}
	// get thread handle passing in the tid
	thread = OpenThread(THREAD_ALL_ACCESS, FALSE, tid);
	if(thread == NULL) {
		printf("Failed to open the main thread, press any key to exit.\n");
		getchar();
		return 0;
	}

	// find base addresses and store them
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
	if(snapshot == INVALID_HANDLE_VALUE) {
		printf("Failed to create snapshot of modules, press any key to exit.\n");
		getchar();
		return 0;
	}

	MODULEENTRY32 entry;
	entry.dwSize = sizeof(MODULEENTRY32);
	if(Module32First(snapshot, &entry)) {
		do {
			if(_wcsicmp(entry.szModule, HAT_EXE_NAME) == 0) {
				hat_address = entry.modBaseAddr;
				break;
			}
		} while(Module32Next(snapshot, &entry));
	}
	CloseHandle(snapshot);

	if(hat_address == NULL) {
		printf("Failed to find base address of HatinTimeGame.exe, press any key to exit.\n");
		getchar();
		return 0;
	}

	return 1;
}

struct hotkey* parse_keybinds(const char* path, unsigned int* out_num) {
	*out_num = 0;
	size_t hotkeys_size = sizeof(struct hotkey) * 8;
	struct hotkey* hotkeys = realloc(NULL, hotkeys_size);
	if(hotkeys == NULL) {
		printf("Unable to allocate buffer for hotkey data...\n");
		return NULL;
	}

	FILE* txtf;
	fopen_s(&txtf, path, "r");
	if(txtf == NULL) {
		printf("Unable to open file '%s'...\n", path);
		return NULL;
	}

	fseek(txtf, 0, SEEK_END);
	size_t txtsize = ftell(txtf);
	fseek(txtf, 0, SEEK_SET);

	char* txtbuf = malloc(txtsize + 1);
	if(txtbuf == NULL) {
		printf("Unable to allocate buffer for file '%s'...\n", path);
		fclose(txtf);
		return NULL;
	}

	fread(txtbuf, txtsize, 1, txtf);
	fclose(txtf);
	txtbuf[txtsize] = '\0';

	char* txtend = txtbuf + txtsize;
	while(txtbuf < txtend - 1) {
		if(*out_num * sizeof(struct hotkey) > hotkeys_size) {
			hotkeys_size *= 2;

			struct hotkey* new_loc = realloc(hotkeys, hotkeys_size);
			if(new_loc == NULL) {
				printf("Unable to increase size of the hotkey data buffer...\n");
				free(hotkeys);
				return NULL;
			}

			hotkeys = new_loc;
		}

		hotkeys[*out_num].vk = strtoul(txtbuf, &txtbuf, 0);
		txtbuf++;
		hotkeys[*out_num].duration = !i_am_testing ? 400.0f : strtof(txtbuf, NULL);
		(*out_num)++;

		while(*txtbuf == '\r' || *txtbuf == '\n') {
			txtbuf++;
		}
	}

	free(txtend - txtsize);

	return hotkeys;
}

int main(int argc, char** argv) {
	int always = 1;

	char* hotkey_path = "keybinds.txt";

	for(int i = 1; i < argc; i++) {
		if(strncmp(argv[i], "--i-am-testing", sizeof("--i-am-testing") - 1) == 0) {
			i_am_testing = 1;
		}
		else {
			hotkey_path = argv[i];
		}
	}

	unsigned int game_open = 0;
	unsigned int hotkey_num;
	struct hotkey* hotkeys = NULL;

	while(1) {
		if(!is_hat_open() || always) {
			always = 0;
			game_open = 0;
			printf("Waiting for the game to open...\n");
			
			while(!init()) {
				Sleep(1000);
			}

			pe_ts = get_pe_ts(hat_address);

			switch(pe_ts) {
				case DLC15_8061143192026666389: fps_path = fps_paths[0]; break;
				case DLC21_242922671485761424: fps_path = fps_paths[1]; break;
				case DLC231_7770543545116491859: fps_path = fps_paths[2]; break;
				case DLC232_5506509173732835905: fps_path = fps_paths[3]; break;
				default: {
					printf("Your version of the game is not supported (%u), doing nothing.\n", pe_ts);
					continue;
				}
			}

			do {
				Sleep(500);
				fps_address = resolve_ptr_path(process, hat_address, fps_path);
			}
			while(fps_address < (BYTE*)0x10000); // arbitrary

			if(hotkeys != NULL) {
				free(hotkeys);
			}

			hotkeys = parse_keybinds(hotkey_path, &hotkey_num);
			if(hotkeys == NULL) {
				printf("Keybind parsing error, press any key to exit.\n");
				getchar();
				return 0;
			}

			game_open = 1;
			printf("Game opened!\n");
		}

		if(GetAsyncKeyState('J') & 0x8000) {
			always = 1;
		}
		
		if(is_hat_open() && game_open) {
			for(unsigned int i = 0; i < hotkey_num; i++) {
				if(GetAsyncKeyState(hotkeys[i].vk) & 0x8000) {
					printf("Lagging for %fms with key 0x%02X\n", hotkeys[i].duration, hotkeys[i].vk);
					float orig_fps;
					float new_fps = 1000.0f / hotkeys[i].duration;

					ReadProcessMemory(process, fps_address, &orig_fps, sizeof(orig_fps), NULL);
					WriteProcessMemory(process, fps_address, &new_fps, sizeof(new_fps), NULL);

					Sleep((DWORD)(hotkeys[i].duration * 0.3f));
					WriteProcessMemory(process, fps_address, &orig_fps, sizeof(orig_fps), NULL);
				}
			}
		}

		Sleep(3);
	}

	return 0;
}