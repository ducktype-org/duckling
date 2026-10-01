/* Mimics the shapes of SDL3's API that a binding has to get right. */
/* This is C: C++ formatting (digit separators) would break it. */
/* clang-format off */
#ifndef SDL_LIKE_H
#define SDL_LIKE_H

#include <stdbool.h>
#include <stdint.h>

#define SL_INIT_VIDEO 0x00000020u
#define SL_WINDOW_RESIZABLE SL_UINT64_C(0x0000000000000020)
#define SL_UINT64_C(c) c##ULL
#define SL_SCALE 1.5
#define SL_NEGATIVE (-3)
#define SL_EMPTY
#define SL_STRING "not a number"

typedef uint32_t SL_InitFlags;

typedef enum SL_EventType {
	SL_EVENT_FIRST = 0,
	SL_EVENT_QUIT  = 0x100,
	SL_EVENT_KEY_DOWN = 0x300,
} SL_EventType;

typedef struct SL_Window SL_Window;

typedef struct SL_Rect {
	int x, y;
	int w, h;
} SL_Rect;

typedef struct SL_FRect {
	float x, y, w, h;
} SL_FRect;

typedef struct SL_KeyboardEvent {
	uint32_t type;
	uint64_t timestamp;
	int32_t  key;
	bool     down;
} SL_KeyboardEvent;

typedef struct SL_QuitEvent {
	uint32_t type;
	uint64_t timestamp;
} SL_QuitEvent;

typedef union SL_Event {
	uint32_t         type;
	SL_KeyboardEvent key;
	SL_QuitEvent     quit;
	uint8_t          padding[64];
} SL_Event;

typedef struct SL_Tagged {
	int kind;
	union {
		int   i;
		float f;
	};
	struct {
		short lo, hi;
	};
} SL_Tagged;

typedef struct SL_Flags {
	unsigned visible : 1;
	unsigned mode : 3;
	signed   delta : 4;
	unsigned wide : 20;
} SL_Flags;

#pragma pack(push, 1)
typedef struct SL_Packed {
	uint8_t  tag;
	uint32_t value;
} SL_Packed;
#pragma pack(pop)

typedef void (*SL_LogFunction)(void* userdata, int priority, const char* message);

bool       SL_Init(SL_InitFlags flags);
void       SL_Quit(void);
SL_Window* SL_CreateWindow(const char* title, int w, int h, uint64_t flags);
void       SL_DestroyWindow(SL_Window* window);
bool       SL_GetWindowSize(SL_Window* window, int* w, int* h);
bool       SL_PushEvent(SL_Event* event);
bool       SL_PollEvent(SL_Event* event);
SL_Rect    SL_MakeRect(int x, int y, int w, int h);
float      SL_RectArea(SL_FRect rect);
void       SL_SetLogOutputFunction(SL_LogFunction callback, void* userdata);
void       SL_Log(const char* fmt, ...);
int        SL_TaggedSum(const SL_Tagged* tagged);
unsigned   SL_FlagsWide(SL_Flags flags);
uint32_t   SL_PackedValue(const SL_Packed* packed);
int        match(int in);

static inline int SL_Inline(void) { return 1; }

#endif
/* clang-format on */
