
#include <sal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unzip.h"
#include "zip.h"
#include "menu.h"
#include "snes9x.h"
#include "memmap.h"
#include "apu.h"
#include "gfx.h"
#include "soundux.h"
#include "snapshot.h"
#include "scaler.h"

#define SNES_SCREEN_WIDTH  256
#define SNES_SCREEN_HEIGHT 192

#define FIXED_POINT 0x10000UL
#define FIXED_POINT_REMAINDER 0xffffUL
#define FIXED_POINT_SHIFT 16

static struct MENU_OPTIONS mMenuOptions;
static int mEmuScreenHeight;
static int mEmuScreenWidth;
static char mRomName[SAL_MAX_PATH]={""};
static u32 mLastRate=0;

static s8 mFpsDisplay[16]={""};
static s8 mVolumeDisplay[16]={""};
static s8 mQuickStateDisplay[16]={""};
static u32 mFps=0;
static u32 mLastTimer=0;
static u32 mEnterMenu=0;
static u32 mLoadRequested=0;
static u32 mSaveRequested=0;
static u32 mQuickStateTimer=0;
static u32 mVolumeTimer=0;
static u32 mVolumeDisplayTimer=0;
static u32 mFramesCleared=0;
static u32 mInMenu=0;

static int S9xCompareSDD1IndexEntries (const void *p1, const void *p2)
{
    return (*(uint32 *) p1 - *(uint32 *) p2);
}

bool JustifierOffscreen (void)
{
	return true;
}

void JustifierButtons (uint32&)
{
}

void S9xProcessSound (unsigned int)
{
}

extern "C"
{

void S9xExit ()
{
}

u32 SamplesDoneThisFrame = 0;

void S9xGenerateSound (void)
{
	so.err_counter += so.err_rate;
	if ((Settings.SoundSync >= 2 && so.err_counter >= FIXED_POINT)
	 || (Settings.SoundSync == 1 && so.err_counter >= FIXED_POINT * 128))
	{
		u32 SamplesThisRun = so.err_counter >> FIXED_POINT_SHIFT;
		so.err_counter &= FIXED_POINT_REMAINDER;
		sal_AudioGenerate(SamplesThisRun);
		SamplesDoneThisFrame += SamplesThisRun;
	}
}

void S9xSetPalette ()
{

}

void S9xExtraUsage ()
{
}
	
void S9xParseArg (char **argv, int &index, int argc)
{	
}

bool8 S9xOpenSnapshotFile (const char *fname, bool8 read_only, STREAM *file)
{
	if (read_only)
	{
		if (*file = OPEN_STREAM(fname,"rb")) 
			return(TRUE);
	}
	else
	{
		if (*file = OPEN_STREAM(fname,"w+b")) 
			return(TRUE);
	}

	return (FALSE);	
}

const char* S9xGetSnapshotDirectory (void)
{
	return sal_DirectoryGetHome();
}
	
void S9xCloseSnapshotFile (STREAM file)
{
	CLOSE_STREAM(file);
}

void S9xMessage (int /* type */, int /* number */, const char *message)
{
	//MenuMessageBox("PocketSnes has encountered an error",(s8*)message,"",MENU_MESSAGE_BOX_MODE_PAUSE);
}

void erk (void)
{
      S9xMessage (0,0, "Erk!");
}

const char *osd_GetPackDir(void)
{
      S9xMessage (0,0,"get pack dir");
      return ".";
}

void S9xLoadSDD1Data (void)
{

}

u16 IntermediateScreen[SNES_WIDTH * SNES_HEIGHT_EXTENDED];

bool8_32 S9xInitUpdate ()
{
	if (mInMenu) return TRUE;

	GFX.Screen = (u8*) IntermediateScreen; /* replacement needed after loading the saved states menu */
	return TRUE;
}

bool8_32 S9xDeinitUpdate (int Width, int Height, bool8_32)
{
	if(mInMenu) return TRUE;

	// After returning from the menu, clear the background of 3 frames.
	// This prevents remnants of the menu from appearing.
	if (mFramesCleared < 3)
	{
		sal_VideoClear(0);
		mFramesCleared++;
	}

	if (mMenuOptions.fullScreen == 1)
	{
		upscale_p((uint32_t*) sal_VideoGetBuffer(), (uint32_t*) IntermediateScreen, SNES_WIDTH);
	}
	if (mMenuOptions.fullScreen == 2)
	{
		upscale_256x224_to_320x240_bilinearish((uint32_t*) sal_VideoGetBuffer() + 160, (uint32_t*) IntermediateScreen, SNES_WIDTH);
	}
	if (mMenuOptions.fullScreen == 0)
	{
		u32 y, pitch = sal_VideoGetPitch();
		u8 *src = (u8*) IntermediateScreen, *dst = (u8*) sal_VideoGetBuffer() + ((SAL_SCREEN_WIDTH - SNES_WIDTH) / 2 + (((SAL_SCREEN_HEIGHT - SNES_HEIGHT) / 2) * SAL_SCREEN_WIDTH)) * sizeof(u16);
		for (y = 0; y < SNES_HEIGHT; y++)
		{
			memcpy(dst, src, SNES_WIDTH * sizeof(u16));
			src += SNES_WIDTH * sizeof(u16);
			dst += pitch;
		}
	}

	u32 newTimer;
	if (mMenuOptions.showFps) 
	{
		mFps++;
		newTimer=sal_TimerRead();
		if(newTimer-mLastTimer>Memory.ROMFramesPerSecond)
		{
			mLastTimer=newTimer;
			sprintf(mFpsDisplay,"FPS: %d / %d", mFps, Memory.ROMFramesPerSecond);
			mFps=0;
		}
		
		sal_VideoDrawRect(0,0,13*8,8,SAL_RGB(0,0,0));
		sal_VideoPrint(0,0,mFpsDisplay,SAL_RGB(31,31,31));
	}

	if(mVolumeDisplayTimer>0)
	{
		sal_VideoDrawRect(100,0,8*8,8,SAL_RGB(0,0,0));
		sal_VideoPrint(100,0,mVolumeDisplay,SAL_RGB(31,31,31));
	}

	if(mQuickStateTimer>0)
	{
		sal_VideoDrawRect(200,0,8*8,8,SAL_RGB(0,0,0));
		sal_VideoPrint(200,0,mQuickStateDisplay,SAL_RGB(31,31,31));
	}

	sal_VideoFlip(0);
}

const char *S9xGetFilename (const char *ex)
{
	static char dir [SAL_MAX_PATH];
	char fname [SAL_MAX_PATH];
	char ext [SAL_MAX_PATH];

	sal_DirectorySplitFilename(Memory.ROMFilename, dir, fname, ext);
	strcpy(dir, sal_DirectoryGetHome());
	sal_DirectoryCombine(dir,fname);
	strcat (dir, ex);

	return (dir);
}

uint32 S9xReadJoypad (int which1)
{
	uint32 val=0x80000000;
	if (mInMenu) return val;
	if (which1 != 0) return val;

	u32 joy = sal_InputPoll();
	
	if ((joy & SAL_INPUT_SELECT)&&(joy & SAL_INPUT_START))	
	{
		mEnterMenu = 1;		
		return val;
	}

#if 0
	if ((joy & SAL_INPUT_L)&&(joy & SAL_INPUT_R)&&(joy & SAL_INPUT_UP))
	{
		if(mVolumeTimer==0)
		{
			mMenuOptions.volume++;
			if(mMenuOptions.volume>31) mMenuOptions.volume=31;
			sal_AudioSetVolume(mMenuOptions.volume,mMenuOptions.volume);
			mVolumeTimer=5;
			mVolumeDisplayTimer=60;
			sprintf(mVolumeDisplay,"Vol: %d",mMenuOptions.volume);
		}
		return val;
	}

	if ((joy & SAL_INPUT_L)&&(joy & SAL_INPUT_R)&&(joy & SAL_INPUT_DOWN))
	{
		if(mVolumeTimer==0)
		{
			mMenuOptions.volume--;
			if(mMenuOptions.volume>31) mMenuOptions.volume=0;
			sal_AudioSetVolume(mMenuOptions.volume,mMenuOptions.volume);
			mVolumeTimer=5;
			mVolumeDisplayTimer=60;
			sprintf(mVolumeDisplay,"Vol: %d",mMenuOptions.volume);
		}
		return val;
	}
#endif

	if (joy & SAL_INPUT_Y) val |= SNES_Y_MASK;
	if (joy & SAL_INPUT_A) val |= SNES_A_MASK;
	if (joy & SAL_INPUT_B) val |= SNES_B_MASK;
	if (joy & SAL_INPUT_X) val |= SNES_X_MASK;
		
	if (joy & SAL_INPUT_UP) 	val |= SNES_UP_MASK;
	if (joy & SAL_INPUT_DOWN) 	val |= SNES_DOWN_MASK;
	if (joy & SAL_INPUT_LEFT) 	val |= SNES_LEFT_MASK;
	if (joy & SAL_INPUT_RIGHT)	val |= SNES_RIGHT_MASK;
	if (joy & SAL_INPUT_START) 	val |= SNES_START_MASK;
	if (joy & SAL_INPUT_SELECT) 	val |= SNES_SELECT_MASK;
	if (joy & SAL_INPUT_L) 		val |= SNES_TL_MASK;
	if (joy & SAL_INPUT_R) 		val |= SNES_TR_MASK;

	return val;
}

bool8 S9xReadMousePosition (int /* which1 */, int &/* x */, int & /* y */,
		    uint32 & /* buttons */)
{
	S9xMessage (0,0,"read mouse");
	return (FALSE);
}

bool8 S9xReadSuperScopePosition (int & /* x */, int & /* y */,
				 uint32 & /* buttons */)
{
      S9xMessage (0,0,"read scope");
      return (FALSE);
}

const char *S9xGetFilenameInc (const char *e)
{
     S9xMessage (0,0,"get filename inc");
     return e;
}

void S9xSyncSpeed(void)
{
      //S9xMessage (0,0,"sync speed");
}

const char *S9xBasename (const char *f)
{
      const char *p;

      S9xMessage (0,0,"s9x base name");

      if ((p = strrchr (f, '/')) != NULL || (p = strrchr (f, '\\')) != NULL)
         return (p + 1);

      return (f);
}

void PSNESForceSaveSRAM (void)
{
	if(mRomName[0] != 0 && Memory.SaveSRAM ((s8*)S9xGetFilename (".srm")))
	{
		sync();
	}
}

void S9xSaveSRAM (int showWarning)
{
	if (CPU.SRAMModified)
	{
		if(Memory.SaveSRAM ((s8*)S9xGetFilename (".srm")))
		{
			sync();
		}
		else
		{
			MenuMessageBox("Saving SRAM","Failed!","",MENU_MESSAGE_BOX_MODE_PAUSE);
		}
	}
	else if(showWarning)
	{
		MenuMessageBox("SRAM saving ignored","No changes have been made to SRAM","",MENU_MESSAGE_BOX_MODE_MSG);
	}
}



}

bool8_32 S9xOpenSoundDevice(int a, unsigned char b, int c)
{

}

void S9xAutoSaveSRAM (void)
{
	if (mMenuOptions.autoSaveSram)
	{
		Memory.SaveSRAM (S9xGetFilename (".srm"));
		// sync(); // Only sync at exit or with a ROM change
	}
}

void S9xLoadSRAM (void)
{
	Memory.LoadSRAM ((s8*)S9xGetFilename (".srm"));
}

static u32 LastAudioRate = 0;
static u32 LastStereo = 0;

static
int Run(int sound)
{
  	int skip=0, done=0, doneLast=0,aim=0,i;
	Settings.NextAPUEnabled = Settings.APUEnabled = sound;
	Settings.SoundSync = mMenuOptions.soundSync;
	sal_TimerInit(Settings.FrameTime);
	done=sal_TimerRead()-1;

	if (sound) {
		/*
		Settings.SoundPlaybackRate = mMenuOptions.soundRate;
		Settings.Stereo = mMenuOptions.stereo ? TRUE : FALSE;
		*/
#ifndef FOREVER_16_BIT_SOUND
		Settings.SixteenBitSound=true;
#endif

		if (LastAudioRate != mMenuOptions.soundRate || LastStereo != mMenuOptions.stereo)
		{
			if (LastAudioRate != 0)
			{
				sal_AudioClose();
			}
			sal_AudioInit(mMenuOptions.soundRate, 16,
						mMenuOptions.stereo, Memory.ROMFramesPerSecond);

			S9xInitSound (mMenuOptions.soundRate,
						mMenuOptions.stereo, sal_AudioGetSamplesPerFrame() * sal_AudioGetBytesPerSample());
			S9xSetPlaybackRate(mMenuOptions.soundRate);
			LastAudioRate = mMenuOptions.soundRate;
		}
		S9xSetSoundMute (FALSE);
		sal_AudioResume();

	} else {
		S9xSetSoundMute (TRUE);
	}

  	while(!mEnterMenu) 
  	{
		for (i=0;i<10;i++)
		{
			aim=sal_TimerRead();
			if (done < aim)
			{
				done++;
				if (mMenuOptions.frameSkip == 0) //Auto
					IPPU.RenderThisFrame = (done >= aim);
				else if (IPPU.RenderThisFrame = (--skip <= 0))
					skip = mMenuOptions.frameSkip;

				//Run SNES for one glorious frame
				S9xMainLoop ();

				sal_AudioGenerate(sal_AudioGetSamplesPerFrame() - SamplesDoneThisFrame);
			}
			if (done>=aim) break; // Up to date now
			if (mEnterMenu) break;
		}
		done=aim; // Make sure up to date
  	}

	if (sound)
		sal_AudioPause();

	mEnterMenu=0;
	return mEnterMenu;

}

static inline int RunSound(void)
{
	return Run(1);
}

static inline int RunNoSound(void)
{
	return Run(0);
}

static 
int SnesRomLoad()
{
	char filename[SAL_MAX_PATH+1];
	int check;
	char text[256];
	FILE *stream=NULL;
  
    	MenuMessageBox("Loading ROM...",mRomName,"",MENU_MESSAGE_BOX_MODE_MSG);

	if (!Memory.LoadROM (mRomName))
	{
		MenuMessageBox("Loading ROM",mRomName,"Failed!",MENU_MESSAGE_BOX_MODE_PAUSE);
		return SAL_ERROR;
	}
	
	MenuMessageBox("Done loading the ROM",mRomName,"",MENU_MESSAGE_BOX_MODE_MSG);

	S9xReset();
	S9xResetSound(1);
	S9xLoadSRAM();
	return SAL_OK;
}

int SnesInit()
{
	ZeroMemory (&Settings, sizeof (Settings));

	Settings.JoystickEnabled = FALSE;
	Settings.SoundPlaybackRate = 44100;
	Settings.Stereo = TRUE;
	Settings.SoundBufferSize = 0;
	Settings.CyclesPercentage = 100;
	Settings.DisableSoundEcho = FALSE;
	Settings.APUEnabled = TRUE;
	Settings.H_Max = SNES_CYCLES_PER_SCANLINE;
	Settings.SkipFrames = AUTO_FRAMERATE;
	Settings.Shutdown = Settings.ShutdownMaster = TRUE;
	Settings.FrameTimePAL = 20000;
	Settings.FrameTimeNTSC = 16667;
	Settings.FrameTime = Settings.FrameTimeNTSC;
	// Settings.DisableSampleCaching = FALSE;
	Settings.DisableMasterVolume = TRUE;
	Settings.Mouse = FALSE;
	Settings.SuperScope = FALSE;
	Settings.MultiPlayer5 = FALSE;
	//	Settings.ControllerOption = SNES_MULTIPLAYER5;
	Settings.ControllerOption = 0;

	Settings.InterpolatedSound = TRUE;
	Settings.StarfoxHack = TRUE;
	
	Settings.ForceTransparency = FALSE;
	Settings.Transparency = TRUE;
#ifndef FOREVER_16_BIT
	Settings.SixteenBit = TRUE;
#endif
	
	Settings.SupportHiRes = FALSE;
	Settings.NetPlay = FALSE;
	Settings.ServerName [0] = 0;
	Settings.AutoSaveDelay = 1;
	Settings.ApplyCheats = TRUE;
	Settings.TurboMode = FALSE;
	Settings.TurboSkipFrames = 15;
	Settings.ThreadSound = FALSE;
	Settings.SoundSync = 1;
	Settings.FixFrequency = TRUE;
	//Settings.NoPatch = true;		

	Settings.SuperFX = TRUE;
	Settings.DSP1Master = TRUE;
	Settings.SA1 = TRUE;
	Settings.C4 = TRUE;
	Settings.SDD1 = TRUE;

	GFX.Screen = (uint8*) IntermediateScreen;
	GFX.RealPitch = GFX.Pitch = 256 * sizeof(u16);
	
	GFX.SubScreen = (uint8 *)malloc(GFX.RealPitch * 480 * 2); 
	GFX.ZBuffer =  (uint8 *)malloc(GFX.RealPitch * 480 * 2); 
	GFX.SubZBuffer = (uint8 *)malloc(GFX.RealPitch * 480 * 2);
	GFX.Delta = (GFX.SubScreen - GFX.Screen) >> 1;
	GFX.PPL = GFX.Pitch >> 1;
	GFX.PPLx2 = GFX.Pitch;
	GFX.ZPitch = GFX.Pitch >> 1;
	
	if (Settings.ForceNoTransparency)
         Settings.Transparency = FALSE;

#ifndef FOREVER_16_BIT
	if (Settings.Transparency)
         Settings.SixteenBit = TRUE;
#endif

	Settings.HBlankStart = (256 * Settings.H_Max) / SNES_HCOUNTER_MAX;

	if (!Memory.Init () || !S9xInitAPU())
	{
		S9xMessage (0,0,"Failed to init memory");
		return SAL_ERROR;
	}

	//S9xInitSound ();
	
	//S9xSetRenderPixelFormat (RGB565);
	S9xSetSoundMute (TRUE);

	if (!S9xGraphicsInit ())
	{
         	S9xMessage (0,0,"Failed to init graphics");
		return SAL_ERROR;
	}

	return SAL_OK;
}

void _makepath (char *path, const char *, const char *dir,
	const char *fname, const char *ext)
{
	if (dir && *dir)
	{
		strcpy (path, dir);
		strcat (path, "/");
	}
	else
	*path = 0;
	strcat (path, fname);
	if (ext && *ext)
	{
		strcat (path, ".");
		strcat (path, ext);
	}
}

void _splitpath (const char *path, char *drive, char *dir, char *fname,
	char *ext)
{
	*drive = 0;

	char *slash = strrchr ((char*)path, '/');
	if (!slash)
		slash = strrchr ((char*)path, '\\');

	char *dot = strrchr ((char*)path, '.');

	if (dot && slash && dot < slash)
		dot = NULL;

	if (!slash)
	{
		strcpy (dir, "");
		strcpy (fname, path);
		if (dot)
		{
			*(fname + (dot - path)) = 0;
			strcpy (ext, dot + 1);
		}
		else
			strcpy (ext, "");
	}
	else
	{
		strcpy (dir, path);
		*(dir + (slash - path)) = 0;
		strcpy (fname, slash + 1);
		if (dot)
		{
			*(fname + (dot - slash) - 1) = 0;
			strcpy (ext, dot + 1);
		}
		else
			strcpy (ext, "");
	}
} 

extern "C"
{

int mainEntry(int argc, char* argv[])
{
	int ref = 0;

	s32 event=EVENT_NONE;

	sal_Init();
	sal_VideoInit(16,SAL_RGB(0,0,0),Memory.ROMFramesPerSecond);

	mRomName[0]=0;
	if (argc >= 2) 
 		strcpy(mRomName, argv[1]); // Record ROM name

	MenuInit(sal_DirectoryGetHome(), &mMenuOptions);


	if(SnesInit() == SAL_ERROR)
	{
		sal_Reset();
		return 0;
	}

	while(1)
	{
		mInMenu=1;
		event=MenuRun(mRomName);
		mInMenu=0;

		if(event==EVENT_LOAD_ROM)
		{
			if (mRomName[0] != 0)
			{
				MenuMessageBox("Saving SRAM...","","",MENU_MESSAGE_BOX_MODE_MSG);
				PSNESForceSaveSRAM();
			}
			if(SnesRomLoad() == SAL_ERROR) 
			{
				mRomName[0] = 0;
				MenuMessageBox("Failed to load ROM",mRomName,"Press any button to continue", MENU_MESSAGE_BOX_MODE_PAUSE);
				sal_Reset();
		    		return 0;
			}
			else
			{
				event=EVENT_RUN_ROM;
		  	}
		}

		if(event==EVENT_RESET_ROM)
		{
			S9xReset();
			event=EVENT_RUN_ROM;
		}

		if(event==EVENT_RUN_ROM)
		{
			if(mMenuOptions.fullScreen)
			{
				sal_VideoSetScaling(SNES_WIDTH,SNES_HEIGHT);
			}

			if(mMenuOptions.transparency)	Settings.Transparency = TRUE;
			else Settings.Transparency = FALSE;

			sal_AudioSetVolume(mMenuOptions.volume,mMenuOptions.volume);
			sal_CpuSpeedSet(mMenuOptions.cpuSpeed);	
			mFramesCleared = 0;
			if(mMenuOptions.soundEnabled) 	
				RunSound();
			else	RunNoSound();

			event=EVENT_NONE;
		}

		if(event==EVENT_EXIT_APP) break;	
	}

	MenuMessageBox("Saving SRAM...","","",MENU_MESSAGE_BOX_MODE_MSG);
	PSNESForceSaveSRAM();
	
	S9xGraphicsDeinit();
	S9xDeinitAPU();
	Memory.Deinit();

	free(GFX.SubZBuffer);
	free(GFX.ZBuffer);
	free(GFX.SubScreen);
	GFX.SubZBuffer=NULL;
	GFX.ZBuffer=NULL;
	GFX.SubScreen=NULL;

	sal_Reset();
  	return 0;
}

}






