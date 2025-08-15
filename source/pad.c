#include <stdio.h>
#include <unistd.h>
#include <ogc/video.h>

#include "pad.h"

static int input_initialized = 0;

#define INPUT_WIIMOTE
#define INPUT_GCN
#define INPUT_USB_KEYBOARD
// #define INPUT_USE_FACEBTNS

/* <Includes> */
#ifdef INPUT_WIIMOTE
#include <wiiuse/wpad.h>
#endif

#ifdef INPUT_GCN
#include <ogc/pad.h>
#endif

#ifdef INPUT_USB_KEYBOARD
#include <wiikeyboard/usbkeyboard.h>
#endif

#ifdef INPUT_USE_FACEBTNS
#include <ogc/lwp_watchdog.h>
/* eh. we have AHB access. Lol. */
#include <ogc/machine/processor.h>
#endif
/* </Includes> */

/* <Globals> */
#ifdef INPUT_USB_KEYBOARD
/* USB Keyboard stuffs */
static lwp_t kbd_thread_hndl = LWP_THREAD_NULL;
static volatile bool kbd_thread_should_run = false;
static uint32_t kbd_buttons;

static const uint16_t keyboardButtonMap[0x100] = {
	[0x1B] = INPUT_X,
	[0x1C] = INPUT_Y,

	[0x28] = INPUT_A, // Enter
	[0x29] = INPUT_HOME, // ESC
	[0x2A] = INPUT_B,

	[0x2D] = 0, // -
	[0x2E] = INPUT_START, // +

	[0x4A] = INPUT_HOME,

	[0x4F] = INPUT_RIGHT,
	[0x50] = INPUT_LEFT,
	[0x51] = INPUT_DOWN,
	[0x52] = INPUT_UP,

	[0x56] = 0, // - (Numpad)
	[0x57] = INPUT_START, // + (Numpad)
	[0x58] = INPUT_A,
};

// from Priiloader (/tools/Dacoslove/source/Input.cpp (!?))
void KBEventHandler(USBKeyboard_event event)
{
	if (event.type != USBKEYBOARD_PRESSED && event.type != USBKEYBOARD_RELEASED)
		return;

	// OSReport("event=%#x, keycode=%#x", event.type, event.keyCode);
	uint32_t button = keyboardButtonMap[event.keyCode];

	if (event.type == USBKEYBOARD_PRESSED)
		kbd_buttons |= button;
	else
		kbd_buttons &= ~button;
}

void* kbd_thread(void* userp) {
	while (kbd_thread_should_run) {
		if (!USBKeyboard_IsConnected() && USBKeyboard_Open(KBEventHandler)) {
			for (int i = 0; i < 3; i++) { USBKeyboard_SetLed(i, 1); usleep(250000); }
		}

		USBKeyboard_Scan();
		usleep(400);
	}

	return NULL;
}

#endif /* INPUT_USB_KEYBOARD */

#ifdef INPUT_USE_FACEBTNS
#define HW_GPIO_IN			0x0D8000E8
#define HW_GPIO_POWER		(1 << 0)
#define HW_GPIO_EJECT		(1 << 6)
#if 0
/* Eh, let's not use interrupts at all. It was nice knowing you, though */

#define HW_PPCIRQFLAG		0x0D000030
#define HW_PPCIRQMASK		0x0D000034

#define HW_IRQ_GPIO			(1 << 11)
#define HW_IRQ_GPIOB		(1 << 10)
#define HW_IRQ_RST			(1 << 17)
#define HW_IRQ_IPC			(1 << 30)


// #define HW_GPIO_INTFLAG		0x0D8000F0
// #define HW_GPIO_OWNER		0x0D8000FC

// note to self: this interrupt will get fired A LOT

void gpio_handler(void) {
	uint32_t time_start = gettime();
	uint32_t down = read32(HW_GPIO_IN) & (HW_GPIO_EJECT | HW_GPIO_POWER);

	if (down == 0 || diff_msec(face_lastinput, time_start) < 200)
		return;

	if (down & HW_GPIO_EJECT) {
		puts("pressed EJECT");
		face_input = INPUT_EJECT;
		face_lastinput = time_start;
	}
	else if (down & HW_GPIO_POWER) {
		puts("pressed POWER");
		face_input = INPUT_POWER;
		face_lastinput = time_start;
	}
}

void rst_handler(void) {
	uint64_t time_start = gettime();

	if (diff_msec(face_lastinput, time_start) < 200)
		return;

	while (SYS_ResetButtonDown()) {
		uint64_t time_now = gettime();
		if (diff_usec(time_start, time_now) >= 100) {
			puts("pressed RESET");
			face_input = INPUT_RESET;
			face_lastinput = time_now;
			break;
		}
	}
}

raw_irq_handler_t ipc_handler = NULL;
void hollywood_irq_handler(uint32_t irq, void *ctx) {
	uint32_t flag = read32(HW_PPCIRQFLAG);

	if (flag & HW_IRQ_IPC) {
		ipc_handler(irq, ctx);
	}

	if (flag & HW_IRQ_RST) {
		write32(HW_PPCIRQFLAG, HW_IRQ_RST);
		// printf("RST interrupt down=%i\n", SYS_ResetButtonDown());
		rst_handler();
	}
/*
	if (flag & HW_IRQ_GPIO) {
		write32(HW_GPIO_INTFLAG, 0xFFFFFF);
		write32(HW_PPCIRQFLAG, HW_IRQ_GPIO);
		// printf("GPIOB interrupt flag=%08x mask=%08x in=%08x\n", read32(HW_GPIOB_INTFLAG), read32(HW_GPIOB_INTMASK), read32(HW_GPIOB_IN));
		gpio_handler();
	}
*/
}

#endif /* 0 */
#endif /* INPUT_USE_FACEBTNS */

#define INPUT_MAP(button, input) [__builtin_ctz(button)] = input

#ifdef INPUT_WIIMOTE
#define WPAD_INPUT_MAP(button, input) INPUT_MAP(WPAD_BUTTON_ ## button, INPUT_ ## input)
#define WPAD_INPUT_MAP_S(button) WPAD_INPUT_MAP(button, button)
#define WPAD_CLASSIC_INPUT_MAP(button, input) INPUT_MAP(WPAD_CLASSIC_BUTTON_ ## button, INPUT_ ## input)
#define WPAD_CLASSIC_INPUT_MAP_S(button) WPAD_CLASSIC_INPUT_MAP(button, button)

static const uint32_t wpadButtonMap[32] = {
	WPAD_INPUT_MAP_S(A),
	WPAD_INPUT_MAP_S(B),
	WPAD_INPUT_MAP(1, X),
	WPAD_INPUT_MAP(2, Y),

	WPAD_INPUT_MAP_S(UP),
	WPAD_INPUT_MAP_S(DOWN),
	WPAD_INPUT_MAP_S(LEFT),
	WPAD_INPUT_MAP_S(RIGHT),

	WPAD_INPUT_MAP(PLUS, START),
	WPAD_INPUT_MAP_S(HOME),

	WPAD_CLASSIC_INPUT_MAP_S(A),
	WPAD_CLASSIC_INPUT_MAP_S(B),
	WPAD_CLASSIC_INPUT_MAP_S(X),
	WPAD_CLASSIC_INPUT_MAP_S(Y),

	WPAD_CLASSIC_INPUT_MAP_S(UP),
	WPAD_CLASSIC_INPUT_MAP_S(DOWN),
	WPAD_CLASSIC_INPUT_MAP_S(LEFT),
	WPAD_CLASSIC_INPUT_MAP_S(RIGHT),

	WPAD_CLASSIC_INPUT_MAP(PLUS, START),
	WPAD_CLASSIC_INPUT_MAP_S(HOME),
};
#undef WPAD_INPUT_MAP
#undef WPAD_INPUT_MAP_S
#undef WPAD_CLASSIC_INPUT_MAP
#undef WPAD_CLASSIC_INPUT_MAP_S
#endif /* INPUT_WIIMOTE */

#ifdef INPUT_GCN
#define PAD_INPUT_MAP(button, input) INPUT_MAP(PAD_BUTTON_ ## button, INPUT_ ## input)
#define PAD_INPUT_MAP_S(button) PAD_INPUT_MAP(button, button)

static const uint32_t padButtonMap[32] = {
	PAD_INPUT_MAP_S(A),
	PAD_INPUT_MAP_S(B),
	PAD_INPUT_MAP_S(X),
	PAD_INPUT_MAP_S(Y),

	PAD_INPUT_MAP_S(UP),
	PAD_INPUT_MAP_S(DOWN),
	PAD_INPUT_MAP_S(LEFT),
	PAD_INPUT_MAP_S(RIGHT),

	PAD_INPUT_MAP_S(START),
};
#undef PAD_INPUT_MAP
#undef PAD_INPUT_MAP_S
#endif /* INPUT_GCN */

#undef INPUT_MAP
/* </Globals> */

void input_init() {
	if (input_initialized)
		return;

#ifdef INPUT_WIIMOTE
	WPAD_Init();
#endif

#ifdef INPUT_GCN
	PAD_Init();
#endif

#ifdef INPUT_USB_KEYBOARD
	USB_Initialize();
	USBKeyboard_Initialize();
	kbd_thread_should_run = true;
	LWP_CreateThread(&kbd_thread_hndl, kbd_thread, 0, 0, 0x800, 0x60);
#endif

	input_initialized = 1;
}

void input_scan() {
#ifdef INPUT_WIIMOTE
	WPAD_ScanPads();
#endif

#ifdef INPUT_GCN
	PAD_ScanPads();
#endif
}

void input_shutdown() {
	if (!input_initialized)
		return;

#ifdef INPUT_WIIMOTE
	WPAD_Flush(WPAD_CHAN_ALL);
	WPAD_Shutdown();
#endif

#ifdef INPUT_USB_KEYBOARD
	kbd_thread_should_run = false;
	USBKeyboard_Close();
	USBKeyboard_Deinitialize();
	if (kbd_thread_hndl != LWP_THREAD_NULL)
		LWP_JoinThread(kbd_thread_hndl, 0);


	kbd_thread_hndl = LWP_THREAD_NULL;
#endif

	input_initialized = 0;
}

uint32_t input_wait(uint32_t mask) {
	uint32_t ret = 0;

	do {
		VIDEO_WaitVSync();
		input_scan();
	} while ((ret = input_read(mask)) == 0);

	return ret;
}

uint32_t input_read(uint32_t mask) {
	uint32_t button = 0;

	if (!mask) mask = ~0;

#ifdef INPUT_WIIMOTE
	uint32_t wm_down = WPAD_ButtonsDown(WPAD_CHAN_0);

	for (int i = 0; i < 32; i++) {
		if (wm_down & (1 << i))
			button |= wpadButtonMap[i];
	}
#endif

#ifdef INPUT_GCN
	uint32_t gcn_down = PAD_ButtonsDown(PAD_CHAN0);

	for (int i = 0; i < 32; i++) {
		if (gcn_down & (1 << i))
			button |= padButtonMap[i];
	}
#endif

#ifdef INPUT_USB_KEYBOARD
	button |= kbd_buttons;
	kbd_buttons = 0;
#endif

#ifdef INPUT_USE_FACEBTNS
	uint64_t time_now = gettime();
	static uint64_t last_input = 0;
	if (!last_input || diff_msec(last_input, time_now) >= 200) {
		if (SYS_ResetButtonDown()) {
			button |= INPUT_RESET;
			last_input = time_now;
		}

		else if (read32(HW_GPIO_IN) & HW_GPIO_POWER) {
			button |= INPUT_POWER;
			last_input = time_now;
		}

		else if (read32(HW_GPIO_IN) & HW_GPIO_EJECT) {
			button |= INPUT_EJECT;
			last_input = time_now;
		}
	}
#endif

	return button & mask;
}



