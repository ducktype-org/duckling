// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "sdl_like.h"

#include <stdlib.h>
#include <string.h>

struct SL_Window {
	int w, h;
};

static SL_Event queue[8];
static int      queued;
static bool     initialized;

bool SL_Init(SL_InitFlags flags) {
	initialized = (flags & SL_INIT_VIDEO) != 0;
	return initialized;
}

void SL_Quit(void) { initialized = false; }

SL_Window* SL_CreateWindow(const char* title, int w, int h, uint64_t flags) {
	if (!initialized || title == NULL || flags != SL_WINDOW_RESIZABLE) return NULL;
	SL_Window* window = malloc(sizeof *window);
	window->w = w;
	window->h = h;
	return window;
}

void SL_DestroyWindow(SL_Window* window) { free(window); }

bool SL_GetWindowSize(SL_Window* window, int* w, int* h) {
	*w = window->w;
	*h = window->h;
	return true;
}

bool SL_PushEvent(SL_Event* event) {
	if (queued == 8) return false;
	queue[queued++] = *event;
	return true;
}

bool SL_PollEvent(SL_Event* event) {
	if (queued == 0) return false;
	*event = queue[0];
	memmove(queue, queue + 1, sizeof(SL_Event) * (size_t)--queued);
	return true;
}

SL_Rect SL_MakeRect(int x, int y, int w, int h) { return (SL_Rect){ x, y, w, h }; }

float SL_RectArea(SL_FRect rect) { return rect.w * rect.h; }

void SL_SetLogOutputFunction(SL_LogFunction callback, void* userdata) {
	(void)callback;
	(void)userdata;
}

void SL_Log(const char* fmt, ...) { (void)fmt; }

int SL_TaggedSum(const SL_Tagged* tagged) { return tagged->kind + tagged->i + tagged->lo + tagged->hi; }

unsigned SL_FlagsWide(SL_Flags flags) { return flags.wide + (unsigned)flags.delta + flags.mode + flags.visible; }

uint32_t SL_PackedValue(const SL_Packed* packed) { return packed->value + packed->tag; }

int SL_ListSum(const SL_List* list) {
	int            sum  = 0;
	const SL_Node* node = list->head;
	do {
		if (node->list == list) sum += node->value;
		node = node->next;
	} while (node != list->head);
	return sum;
}

int match(int in) { return in; }
