#ifdef __HAIKU__
#include <os/BeBuild.h>
#include <SupportDefs.h>

extern uint32 _gSharedObjectHaikuABI = B_HAIKU_VERSION;
extern uint32 _gSharedObjectHaikuVersion = B_HAIKU_ABI;

#endif
