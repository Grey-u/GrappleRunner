// GrappleRunner.cpp - registers our code with Unreal as "the main game module".
// This one line is all a simple game module needs; the real gameplay lives in GrappleRig.cpp.

#include "GrappleRunner.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, GrappleRunner, "GrappleRunner");
