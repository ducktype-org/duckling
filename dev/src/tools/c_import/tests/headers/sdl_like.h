// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

/* Mimics the shapes of SDL3's API that a binding has to get right. */
/* This is C: C++ formatting (digit separators) would break it. */
/* clang-format off */
#ifndef SDL_LIKE_H
#define SDL_LIKE_H

#include <stdbool.h>
#include <stdint.h>

/** Initializes the video subsystem. */
#define SL_INIT_VIDEO 0x00000020u
/* Not a doc comment. */
#define SL_WINDOW_RESIZABLE SL_UINT64_C(0x0000000000000020)
#define SL_UINT64_C(c) c##ULL
#define SL_SCALE 1.5 /**< The display scale. */
/// A negative value,
/// over two lines.
#define SL_NEGATIVE (-3)
#define SL_EMPTY
#define SL_STRING "not a number"

typedef uint32_t SL_InitFlags;

/** The types of events. */
typedef enum SL_EventType {
	SL_EVENT_FIRST = 0, /**< Unused. */

	/** User-requested quit. */
	SL_EVENT_QUIT  = 0x100,
	SL_EVENT_KEY_DOWN = 0x300,
} SL_EventType;

/** A window, only used through a pointer. */
typedef struct SL_Window SL_Window;

/**
 * A rectangle, with the origin at the upper left.
 *
 *     indented example
 *
 * \since This struct is available since SL 1.0.
 */
typedef struct SL_Rect {
	int x, y;
	int w, h; /**< The size. */
} SL_Rect;

typedef struct SL_FRect {
	float x, y, w, h;
} SL_FRect;

typedef struct SL_KeyboardEvent {
	uint32_t type;
	uint64_t timestamp;
	int32_t  key;  /**< The key code. */
	bool     down; ///< Whether the key is pressed.
} SL_KeyboardEvent;

typedef struct SL_QuitEvent {
	uint32_t type;
	uint64_t timestamp;
} SL_QuitEvent;

typedef union SL_Event {
	uint32_t         type;
	SL_KeyboardEvent key;  /**< Keyboard event data. */
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

typedef struct SL_List SL_List;

typedef struct SL_Node {
	int              value;
	struct SL_Node*  next;
	SL_List*         list;
} SL_Node;

struct SL_List {
	SL_Node* head;
};

typedef void (*SL_LogFunction)(void* userdata, int priority, const char* message);

bool       SL_Init(SL_InitFlags flags);
/** Quits — always succeeds. */
void       SL_Quit(void);
/**
 * Creates a window.
 *
 * \param title the title, in UTF-8.
 * \returns the window, or NULL on failure.
 */
SL_Window* SL_CreateWindow(const char* title, int w, int h, uint64_t flags);
/// Destroys a window.
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
int        SL_ListSum(const SL_List* list);
int        match(int in);

static inline int SL_Inline(void) { return 1; }

#endif
/* clang-format on */
