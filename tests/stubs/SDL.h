#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "SDL_gamecontroller.h"
#include "SDL_version.h"
typedef struct SDL_mutex SDL_mutex;
typedef struct SDL_cond SDL_cond;
typedef struct SDL_Thread SDL_Thread;
typedef struct SDL_AudioStream SDL_AudioStream;
#define SDL_LOG_CATEGORY_INPUT 1
#define AUDIO_S16LSB 0x8010
#define AUDIO_S8 0x8008
#define SDL_TICKS_PASSED(a,b) ((Sint32)((b)-(a)) <= 0)
Uint64 SDL_GetPerformanceCounter(void);
Uint64 SDL_GetPerformanceFrequency(void);
Uint32 SDL_GetTicks(void);
SDL_mutex *SDL_CreateMutex(void);
int SDL_LockMutex(SDL_mutex *);
int SDL_UnlockMutex(SDL_mutex *);
void SDL_DestroyMutex(SDL_mutex *);
SDL_cond *SDL_CreateCond(void);
int SDL_CondSignal(SDL_cond *);
int SDL_CondWait(SDL_cond *, SDL_mutex *);
int SDL_CondWaitTimeout(SDL_cond *, SDL_mutex *, Uint32);
void SDL_DestroyCond(SDL_cond *);
SDL_Thread *SDL_CreateThread(int(*fn)(void *), const char *name, void*);
void SDL_WaitThread(SDL_Thread*,int *);
SDL_AudioStream *SDL_NewAudioStream(int,int,int,int,int,int);
void SDL_FreeAudioStream(SDL_AudioStream *);
void SDL_AudioStreamClear(SDL_AudioStream *);
int SDL_AudioStreamAvailable(SDL_AudioStream *);
int SDL_AudioStreamPut(SDL_AudioStream *,const void *, int);
int SDL_AudioStreamGet(SDL_AudioStream *,void *,int);
void *SDL_calloc(size_t,size_t);
void SDL_free(void *);
void SDL_LogWarn(int, const char *, ...);
void SDL_LogInfo(int, const char *, ...);
