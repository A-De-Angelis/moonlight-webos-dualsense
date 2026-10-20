#pragma once
#include <stdint.h>
typedef uint8_t Uint8; typedef uint16_t Uint16; typedef uint32_t Uint32; typedef uint64_t Uint64;
typedef int32_t Sint32;
typedef struct SDL_GameController SDL_GameController;
typedef struct SDL_Joystick SDL_Joystick;
typedef struct { Uint8 data[16]; } SDL_JoystickGUID;
Uint16 SDL_GameControllerGetVendor(SDL_GameController *);
Uint16 SDL_GameControllerGetProduct(SDL_GameController *);
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *);
SDL_JoystickGUID SDL_JoystickGetGUID(SDL_Joystick *);
int SDL_GameControllerRumble(SDL_GameController *, Uint16, Uint16, Uint32);
