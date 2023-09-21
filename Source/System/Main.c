/****************************/
/*    BUGDOM 2 - MAIN 		*/
/* (c)2002 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "windows.h"
#include "3dmath.h"
#include "mainmenu.h"
#include "infobar.h"
#include "vaportrails.h"
#include "tunnel.h"
#include "dialog.h"
#include "internet.h"

extern	Boolean			gDrawLensFlare,gDisableHiccupTimer, gPlayerIsDead;
extern	NewObjectDefinitionType	gNewObjectDefinition;
extern	float			gFramesPerSecond,gFramesPerSecondFrac,gAutoFadeStartDist,gAutoFadeEndDist,gAutoFadeRange_Frac;
extern	float			gTerrainPolygonSize, gDeathTimer;
extern	OGLPoint3D	gCoord;
extern	ObjNode				*gFirstNodePtr;
extern	short		gNumSuperTilesDrawn;
extern	float		gGlobalTransparency;
extern	int			gNumObjectNodes,gSuperTileActiveRange, gNumFoodOnBasket,gNumCaughtFish;
extern	PrefsType	gGamePrefs;
extern	short	gNumTerrainDeformations;
extern	DeformationType	gDeformationList[];
extern	long			gTerrainUnitWidth,gTerrainUnitDepth;
extern	MetaObjectPtr			gBG3DGroupList[MAX_BG3D_GROUPS][MAX_OBJECTS_IN_GROUP];
extern	int			gKindlingCount, gNumDrowningMiceRescued;
extern	Boolean		gBurnKindling, gIgnoreBottleKeySnail, gGameIsRegistered,gSlowCPU;
extern	float					gAnaglyphScaleFactor, gAnaglyphFocallength, gAnaglyphEyeSeparation;


/****************************/
/*    PROTOTYPES            */
/****************************/

static void InitArea(void);
static void InitArea_Exploration(void);
static void CleanupLevel(void);
static void PlayArea(void);
static void PlayArea_Terrain(void);
static void PlayGame(void);
static void UpdateParkFog(void);


/****************************/
/*    CONSTANTS             */
/****************************/

#define	NORMAL_GRAVITY	4500.0f


#define kDefaultNibFileName "main"


/****************************/
/*    VARIABLES             */
/****************************/

short	gPrefsFolderVRefNum;
long	gPrefsFolderDirID;

Boolean				gShareware = false;

float				gDemoVersionTimer = 0;

Boolean				gG4 = false;

float				gGravity = NORMAL_GRAVITY;

Byte				gDebugMode = 0;				// 0 == none, 1 = fps, 2 = all

u_long				gAutoFadeStatusBits;

OGLSetupOutputType		*gGameViewInfoPtr = nil;

PrefsType			gGamePrefs;

FSSpec				gDataSpec;


OGLVector3D			gWorldSunDirection = { .5, -.35, .8};		// also serves as lense flare vector
OGLColorRGBA		gFillColor1 = { .6, .6, .6, 1};

u_long				gGameFrameNum = 0;
float				gGameLevelTimer = 0;

Boolean				gPlayingFromSavedGame = false;
Boolean				gGameOver = false;
Boolean				gLevelCompleted = false;
float				gLevelCompletedCoolDownTimer = 0;

int					gLevelNum;

short				gBestCheckpointNum;
OGLPoint2D			gBestCheckpointCoord;
float				gBestCheckpointAim;

int					gScratch = 0;
float				gScratchF = 0;

u_long				gScore,gLoadedScore;

CFBundleRef 		gBundle = nil;
IBNibRef 			gNibs = nil;


//======================================================================================
//======================================================================================
//======================================================================================


/****************** TOOLBOX INIT  *****************/

void ToolBoxInit(void)
{
OSErr		iErr;
long		createdDirID;


	MyFlushEvents();

			/*****************/
			/* INIT NIB INFO */
			/*****************/

	gBundle = CFBundleGetMainBundle();
	if (gBundle == nil)
		DoFatalAlert("\pToolBoxInit: CFBundleGetMainBundle() failed!");
	if (CreateNibReferenceWithCFBundle(gBundle, CFSTR(kDefaultNibFileName), &gNibs) != noErr)
		DoFatalAlert("\pToolBoxInit: CreateNibReferenceWithCFBundle() failed!");


			/* CHECK PREFERENCES FOLDER */

	iErr = FindFolder(kOnSystemDisk,kPreferencesFolderType,kDontCreateFolder,			// locate the folder
					&gPrefsFolderVRefNum,&gPrefsFolderDirID);
	if (iErr != noErr)
		DoAlert("\pWarning: Cannot locate the Preferences folder.");

	iErr = DirCreate(gPrefsFolderVRefNum,gPrefsFolderDirID,"\pBugdom2",&createdDirID);		// make folder in there



			/* MAKE FSSPEC FOR DATA FOLDER */

	SetDefaultDirectory();							// be sure to get the default directory

	iErr = FSMakeFSSpec(0, 0, "\p:Data:Images", &gDataSpec);
	if (iErr)
		DoFatalAlert("\pThe game's Data folder appears to be missing.  Please reinstall the game.");


			/* DETERMINE IF SHAREWARE OR BOXED VERSION */
			//
			// The boxed version cannot play in demo mode, so it will not
			// have the DemoQuit / DemoExpired image files.
			//

#if (!DEMO) && (!OEM)
	{
		FSSpec	spc;

		if (FSMakeFSSpec(0, 0, "\p:Data:Images:DemoQuit", &spc) == noErr)
			gShareware = true;
		else
			gShareware = false;
	}
#endif

		/* FIRST VERIFY SYSTEM BEFORE GOING TOO FAR */

	VerifySystem();
 	InitInput();



			/********************/
			/* INIT PREFERENCES */
			/********************/

	InitDefaultPrefs();
	LoadPrefs(&gGamePrefs);



			/* BOOT OGL */

	OGL_Boot();


			/* START QUICKTIME */

	EnterMovies();



			/************************************/
            /* SEE IF GAME IS REGISTERED OR NOT */
			/************************************/

#if DEMO || APPSTORE || OEM
	gGameIsRegistered = true;
#else
    CheckGameSerialNumber();
#endif


		/**********************************/
		/* SEE IF SHOULD DO VERSION CHECK */
		/**********************************/

#if !DEMO
	if (gGameIsRegistered || APPSTORE)					// only do HTTP if running full version in registered mode
	{
		DateTimeRec	dateTime;
		u_long		seconds, seconds2;

		GetTime(&dateTime);								// get date time
		DateToSeconds(&dateTime, &seconds);

		DateToSeconds(&gGamePrefs.lastVersCheckDate, &seconds2);

		if ((seconds - seconds2) > 86400)				// see if 1 days have passed since last check
		{
			MyFlushEvents();
			gGamePrefs.lastVersCheckDate = dateTime;	// update time
			SavePrefs();

			ReadHTTPData_VersionInfo();					// do version check (also checks serial #'s)
		}
	}
#endif

			/*********************************/
			/* DO BOOT CHECK FOR SCREEN MODE */
			/*********************************/

	DoScreenModeDialog();

	MyFlushEvents();
}


/************************* INIT DEFAULT PREFS **************************/

void InitDefaultPrefs(void)
{
int			i;
long 		keyboardScript, languageCode;

		/* DETERMINE WHAT LANGUAGE IS ON THIS MACHINE */

	keyboardScript = GetScriptManagerVariable(smKeyScript);
	languageCode = GetScriptVariable(keyboardScript, smScriptLang);

	switch(languageCode)
	{
		case	langFrench:
				gGamePrefs.language 			= LANGUAGE_FRENCH;
				break;

		case	langGerman:
				gGamePrefs.language 			= LANGUAGE_GERMAN;
				break;

		case	langSpanish:
				gGamePrefs.language 			= LANGUAGE_SPANISH;
				break;

		case	langItalian:
				gGamePrefs.language 			= LANGUAGE_ITALIAN;
				break;

		case	langDutch:
				gGamePrefs.language 			= LANGUAGE_DUTCH;
				break;

		default:
				gGamePrefs.language 			= LANGUAGE_ENGLISH;
	}


	gGamePrefs.version				= CURRENT_PREFS_VERS;
	gGamePrefs.difficulty			= 0;
	gGamePrefs.showScreenModeDialog = true;
	gGamePrefs.depth				= 32;
	gGamePrefs.screenWidth			= 800;
	gGamePrefs.screenHeight			= 600;
	gGamePrefs.hz				= 0;
	gGamePrefs.kiddieMode			= false;
	gGamePrefs.deepZ				= true;
	gGamePrefs.lastVersCheckDate.year = 0;
	gGamePrefs.anaglyph				=  false;
	gGamePrefs.anaglyphColor		= false;
	gGamePrefs.dontUseHID			= false;
	gGamePrefs.reserved[0] 			= 0;
	gGamePrefs.reserved[1] 			= 0;
	gGamePrefs.reserved[2] 			= 0;
	gGamePrefs.reserved[3] 			= 0;
	gGamePrefs.reserved[4] 			= 0;
	gGamePrefs.reserved[5] 			= 0;
	gGamePrefs.reserved[6] 			= 0;
	gGamePrefs.reserved[7] 			= 0;

	for (i = 0; i < MAX_HTTP_NOTES; i++)
		gGamePrefs.didThisNote[i] = false;
}


#pragma mark -

/******************** PLAY GAME ************************/

static void PlayGame(void)
{
const short songs[] =
{
	SONG_LEVEL1_GARDEN,				// front yard
	SONG_LEVEL2_POOL,				// back yard
	SONG_LEVEL3_FIDO,				// fido
	SONG_LEVEL4_PLUMBING,			// sewer
	SONG_LEVEL5_PLAYROOM,			// playroom
	SONG_LEVEL6_CLOSET,				// closet
	SONG_LEVEL4_PLUMBING,			// gutter
	SONG_LEVEL8_GARBAGE,			// garbage
	SONG_LEVEL9_BALSA,				// balsa
	SONG_LEVEL10_PARK,				// park
};

			/***********************/
			/* GAME INITIALIZATION */
			/***********************/

	InitPlayerInfo_Game();					// init player info for entire game


			/*********************************/
			/* PLAY THRU LEVELS SEQUENTIALLY */
			/*********************************/

	if (!gPlayingFromSavedGame)				// start on Level 0 if not loading from saved game
	{
		gLevelNum = 0;

#if !DEMO
		if (gGameIsRegistered)					// only allow cheating if game is registered
		{
//			if (GetKeyState(KEY_F10))		// see if do Level cheat
//				if (DoLevelCheatDialog())
//					CleanQuit();
		}
#endif
	}

	GammaFadeOut();

	for (;gLevelNum < NUM_LEVELS; gLevelNum++)
	{
		PlaySong(songs[gLevelNum], true);

				/* DO LEVEL INTRO */

		DoLevelIntro();

		MyFlushEvents();


	        /* LOAD ALL OF THE ART & STUFF */

		InitArea();


			/***********/
	        /* PLAY IT */
	        /***********/

		PlayArea();

			/* CLEANUP LEVEL */

		MyFlushEvents();
		GammaFadeOut();
		CleanupLevel();
		GameScreenToBlack();

		if (DEMO || (!gGameIsRegistered))				// only allow this one level if not registered or is demo
			break;

			/***************/
			/* SEE IF LOST */
			/***************/

		if (gGameOver)									// bail out if game has ended
		{
			DoLoseScreen();
			break;
		}


		/* DO END-LEVEL BONUS SCREEN */

		if (gLevelNum == LEVEL_NUM_PARK)				// if just won game then do win screen first!
			DoWinScreen();
		DoBonusScreen();

	}

#if !DEMO


			/* DO HIGH SCORES */

	NewScore();
#endif

	gPlayingFromSavedGame = false;
}

/************************* PLAY AREA *******************************/

static void PlayArea(void)
{
			/* PREP STUFF */

	UpdateInput();
	CalcFramesPerSecond();
	CalcFramesPerSecond();



	/***********************************************/
	/* PLAY BASED ON THE TYPE OF LEVEL WE'RE DOING */
	/***********************************************/

	switch(gLevelNum)
	{
			/* PLAY TUNNEL LEVELS */

		case	LEVEL_NUM_PLUMBING:
		case	LEVEL_NUM_GUTTER:
				PlayArea_Tunnel();
				break;


			/* PLAY EXPLORATION LEVELS */

		default:
				PlayArea_Terrain();
	}
}



/**************** PLAY AREA: TERRAIN  ************************/
//
// The main loop for a terrain exploration level
//

static void PlayArea_Terrain(void)
{



		/******************/
		/* MAIN GAME LOOP */
		/******************/

	while(true)
	{
		gDemoVersionTimer += gFramesPerSecondFrac;							// count the seconds for Demo or unregistered version


					/* GET CONTROL INFORMATION FOR THIS FRAME */

		UpdateInput();									// read local keys

				/* MOVE OBJECTS */

		MoveEverything();


			/* UPDATE THE TERRAIN */

		DoPlayerTerrainUpdate(gPlayerInfo.camera.cameraLocation.x, gPlayerInfo.camera.cameraLocation.z);


			/* DRAW IT ALL */


		OGL_DrawScene(gGameViewInfoPtr,DrawArea);



			/**************/
			/* MISC STUFF */
			/**************/

			/* SEE IF PAUSED */

		if (GetNewKeyState(KEY_ESC))
			DoPaused();

		if (GetNewKeyState(KEY_F15))								// do screen-saver-safe paused mode
		{
			glFinish();

			do
			{
				EventRecord	e;
				WaitNextEvent(everyEvent,&e, 0, 0);
				UpdateInput();
			}while(!GetNewKeyState(KEY_F15));

			CalcFramesPerSecond();
		}


		CalcFramesPerSecond();

		if (gGameFrameNum == 0)						// if that was 1st frame, then create a fade event
			MakeFadeEvent(true, 1);

		gGameFrameNum++;
		gGameLevelTimer += gFramesPerSecondFrac;


				/* CHEATS */

		if (GetKeyState(KEY_APPLE))
		{
			if (GetNewKeyState(KEY_F10))			// win level cheat
				break;

			if (GetNewKeyState(KEY_HELP))			// get health & stuff
			{
				gPlayerInfo.health = 1.0;
				gPlayerInfo.glidePower = 1.0;
				if (gPlayerInfo.lives < 3)
					gPlayerInfo.lives = 3;
			}
		}

				/* SEE IF LEVEL IS COMPLETED */

		if (gGameOver)													// if we need immediate abort, then bail now
			break;

		if (gLevelCompleted)
		{
			gLevelCompletedCoolDownTimer -= gFramesPerSecondFrac;		// game is done, but wait for cool-down timer before bailing
			if (gLevelCompletedCoolDownTimer <= 0.0f)
				break;
		}

				/* SEE IF RESET PLAYER NOW */

		if (gPlayerIsDead)
		{
			float	oldTimer = gDeathTimer;
			gDeathTimer -= gFramesPerSecondFrac;
			if (gDeathTimer <= 0.0f)
			{
				if (oldTimer > 0.0f)						// if just now crossed zero then start fade
					MakeFadeEvent(false, 1);
				else
				if (gGammaFadePercent <= 0.0f)				// once fully faded out reset player @ checkpoint
					ResetPlayerAtBestCheckpoint();
			}
		}

		gDisableHiccupTimer = false;									// reenable this after the 1st frame

	}

}


/****************** DRAW AREA *******************************/

void DrawArea(OGLSetupOutputType *setupInfo)
{
		/* DRAW OBJECTS & TERAIN */

	DrawObjects(setupInfo);												// draw objNodes which includes fences, terrain, etc.

			/* DRAW MISC */

	DrawShards(setupInfo);												// draw shards
	DrawVaporTrails(setupInfo);											// draw vapor trails
	DrawSparkles(setupInfo);											// draw light sparkles
	DrawLensFlare(setupInfo);											// draw lens flare
	DrawInfobar(setupInfo);												// draw infobar last



}


/******************** MOVE EVERYTHING ************************/

void MoveEverything(void)
{

	MoveVaporTrails();
	MoveObjects();
	MoveSplineObjects();
	MoveShards();
	UpdateCamera();								// update camera

	/* LEVEL SPECIFIC UPDATES */

	switch(gLevelNum)
	{
		case	LEVEL_NUM_GNOMEGARDEN:
				UpdateSprinklerHeads();
				break;

		case	LEVEL_NUM_PLAYROOM:
				UpdateSlotCarRacing();
				break;

		case	LEVEL_NUM_GARBAGE:
				RaiseWater();
				break;

		case	LEVEL_NUM_PARK:
				UpdateParkFog();
				break;

	}

}


/******************* UPDATE PARK FOG ****************************/
//
// Varies the density of the fog based on terrain Y
//

static void UpdateParkFog(void)
{
float	y = GetTerrainY(gPlayerInfo.coord.x, gPlayerInfo.coord.z);
float	start,end;
float	decay = (1.0f / 2600.0f);			// decay based on upper margin

	y -= 600.0f;							// set lower margin of n
	if (y < 0.0f)
		y = 0.0f;


	start = .0f + (y * decay);
	start *= gGameViewInfoPtr->yon;

	end = .8f + (y * decay);
	end *= gGameViewInfoPtr->yon;

	if (start > end)
		start = end;

	glFogf(GL_FOG_START, start);
	glFogf(GL_FOG_END, end);

}


#pragma mark -

/***************** INIT AREA ************************/

static void InitArea(void)
{

		/*********************/
		/* INIT COMMON STUFF */
		/*********************/

	gGameFrameNum 		= 0;
	gGameLevelTimer 	= 0;
	gGameOver 			= false;
	gLevelCompleted 	= false;
	gBestCheckpointNum	= -1;


	gGravity = NORMAL_GRAVITY;					// assume normal gravity


	gPlayerInfo.objNode = nil;

	/***********************************************/
	/* INIT BASED ON THE TYPE OF LEVEL WE'RE DOING */
	/***********************************************/

	switch(gLevelNum)
	{
			/* INIT TUNNEL LEVELS */

		case	LEVEL_NUM_PLUMBING:
		case	LEVEL_NUM_GUTTER:
				InitArea_Tunnels();
				break;

			/* INIT EXPLORATION LEVELS */

		default:
				InitArea_Exploration();
	}


	HideRealCursor();								// do this again to be sure!
	GammaFadeOut();
}




/******************** INIT AREA:  EXPLORATION ***************************/
//
// This does the level initialization for the levels which are the regular "exploration" type.
//

static void InitArea_Exploration(void)
{
OGLSetupInputType	viewDef;


			/*************/
			/* MAKE VIEW */
			/*************/

	SetTerrainScale(DEFAULT_TERRAIN_SCALE);								// set scale to some default for now



			/* SETUP VIEW DEF */

	OGL_NewViewDef(&viewDef);

	viewDef.camera.hither 			= 10;
	viewDef.camera.fov 				= GAME_FOV;


			/* SET ANAGLYPH INFO */

	if (gGamePrefs.anaglyph)
	{
		gAnaglyphScaleFactor 	= 1.0f;
		gAnaglyphFocallength	= 130.0f * gAnaglyphScaleFactor;	// set camera info
		gAnaglyphEyeSeparation 	= 20.0f * gAnaglyphScaleFactor;
	}



	switch(gLevelNum)
	{
		case	LEVEL_NUM_SIDEWALK:
#if APPSTORE
				gSuperTileActiveRange = MAX_SUPERTILE_ACTIVE_RANGE;
#else
				if (gG4)												// better range if we can afford it
					gSuperTileActiveRange = 5;
#endif
				viewDef.styles.useFog			= true;
				viewDef.view.clearColor.r 		= .9;
				viewDef.view.clearColor.g 		= .9;
				viewDef.view.clearColor.b		= .9;
				viewDef.view.clearBackBuffer	= true;
				viewDef.camera.yon 				= (gSuperTileActiveRange * SUPERTILE_SIZE * gTerrainPolygonSize) * .95f;
				viewDef.styles.fogStart			= viewDef.camera.yon * .5f;
				viewDef.styles.fogEnd			= viewDef.camera.yon * 1.2f;
				gDrawLensFlare = true;
				break;

		case	LEVEL_NUM_FIDO:
				viewDef.camera.yon 				*= .8f;

				viewDef.styles.useFog			= true;
				viewDef.view.clearColor.r 		= .12;
				viewDef.view.clearColor.g 		= .08;
				viewDef.view.clearColor.b		= .08;
				viewDef.view.clearBackBuffer	= true;
				viewDef.camera.yon 				= (gSuperTileActiveRange * SUPERTILE_SIZE * gTerrainPolygonSize) * .7f;
				viewDef.styles.fogStart			= viewDef.camera.yon * .1f;
				viewDef.styles.fogEnd			= viewDef.camera.yon * .95f;
				gDrawLensFlare = false;
				break;

		case	LEVEL_NUM_PLAYROOM:
#if APPSTORE
				gSuperTileActiveRange = MAX_SUPERTILE_ACTIVE_RANGE;
#else
				if (gG4)												// better range if we can afford it
					gSuperTileActiveRange = 5;
#endif
				viewDef.styles.useFog			= false;
				viewDef.view.clearBackBuffer	= true;
				viewDef.camera.yon 				= (gSuperTileActiveRange * SUPERTILE_SIZE * gTerrainPolygonSize) * .7f;
				gDrawLensFlare = false;
				break;


		case	LEVEL_NUM_CLOSET:
				viewDef.styles.useFog			= true;
				viewDef.view.clearColor.r 		= .0;
				viewDef.view.clearColor.g 		= .0;
				viewDef.view.clearColor.b		= .1;
				viewDef.view.clearBackBuffer	= true;
				viewDef.camera.yon 				= (gSuperTileActiveRange * SUPERTILE_SIZE * gTerrainPolygonSize) * .7f;
				viewDef.styles.fogStart			= viewDef.camera.yon * .2f;
				viewDef.styles.fogEnd			= viewDef.camera.yon * .95f;
				gDrawLensFlare = false;
				break;

		case	LEVEL_NUM_GARBAGE:
#if APPSTORE
				gSuperTileActiveRange = MAX_SUPERTILE_ACTIVE_RANGE;
#else
				if (gG4)												// better range if we can afford it
					gSuperTileActiveRange = 5;
#endif
				viewDef.styles.useFog			= true;
				viewDef.view.clearColor.r 		= .1;
				viewDef.view.clearColor.g 		= .2;
				viewDef.view.clearColor.b		= .1;
				viewDef.view.clearBackBuffer	= true;
				viewDef.camera.yon 				= (gSuperTileActiveRange * SUPERTILE_SIZE * gTerrainPolygonSize) * .7f;
				viewDef.styles.fogStart			= viewDef.camera.yon * .7f;
				viewDef.styles.fogEnd			= viewDef.camera.yon * 1.1f;
				gDrawLensFlare = true;
				break;



		case	LEVEL_NUM_BALSA:
				SetTerrainScale(DEFAULT_TERRAIN_SCALE*1.6);								// scale the terrain up for this level
				gSuperTileActiveRange = 3;

				viewDef.camera.fov 				= .9;
				viewDef.styles.useFog			= false;
				viewDef.view.clearBackBuffer	= true;
				viewDef.view.clearColor.r 		= 0;
				viewDef.view.clearColor.g 		= 0;
				viewDef.view.clearColor.b		= 0;
				viewDef.camera.hither			= 200;
				viewDef.camera.yon 				= 9000;
				gDrawLensFlare = false;

				if (gGamePrefs.anaglyph)
				{
					gAnaglyphScaleFactor 	= 1.0f;
					gAnaglyphFocallength	= 1300.0f * gAnaglyphScaleFactor;	// set camera info
					gAnaglyphEyeSeparation 	= 55.0f * gAnaglyphScaleFactor;
				}

				break;

		case	LEVEL_NUM_PARK:
#if APPSTORE
				gSuperTileActiveRange = MAX_SUPERTILE_ACTIVE_RANGE;
#else
				if (gG4)												// better range if we can afford it
					gSuperTileActiveRange = 5;
#endif
				viewDef.styles.useFog			= true;
				viewDef.view.clearBackBuffer	= true;
				viewDef.view.clearColor.r 		= 1;
				viewDef.view.clearColor.g 		= 1;
				viewDef.view.clearColor.b		= 1;
				gDrawLensFlare = true;
				break;

		default:
#if APPSTORE
				gSuperTileActiveRange = MAX_SUPERTILE_ACTIVE_RANGE;
#else
				if (gSlowCPU)
					gSuperTileActiveRange = 3;
#endif

				viewDef.styles.useFog			= true;
				viewDef.view.clearColor.r 		= 0;
				viewDef.view.clearColor.g 		= .4;
				viewDef.view.clearColor.b		= 0;
				viewDef.view.clearBackBuffer	= true;
				viewDef.camera.yon 				= (gSuperTileActiveRange * SUPERTILE_SIZE * gTerrainPolygonSize) * .95f;
				viewDef.styles.fogStart			= viewDef.camera.yon * .7f;
				viewDef.styles.fogEnd			= viewDef.camera.yon * 1.0f;
				gDrawLensFlare = true;
	}




		/**************/
		/* SET LIGHTS */
		/**************/

	switch(gLevelNum)
	{
		case	LEVEL_NUM_FIDO:
				viewDef.lights.numFillLights 		= 2;

				viewDef.lights.ambientColor.r 		= .2;
				viewDef.lights.ambientColor.g 		= .1;
				viewDef.lights.ambientColor.b 		= .1;

				viewDef.lights.fillDirection[0].x 	= -.5;
				viewDef.lights.fillDirection[0].y 	= -.2;
				viewDef.lights.fillDirection[0].z 	= .5;
				viewDef.lights.fillColor[0].r 		= 1.0;
				viewDef.lights.fillColor[0].g 		= 1.0;
				viewDef.lights.fillColor[0].b 		= .8;

				viewDef.lights.fillDirection[1].x 	= .4;
				viewDef.lights.fillDirection[1].y 	= -.1;
				viewDef.lights.fillDirection[1].z 	= -.5;
				viewDef.lights.fillColor[1].r 		= .5;
				viewDef.lights.fillColor[1].g 		= .5;
				viewDef.lights.fillColor[1].b 		= .4;
				break;


		case	LEVEL_NUM_SIDEWALK:
				gWorldSunDirection.x = .8;
				gWorldSunDirection.y = -.5;
				gWorldSunDirection.z = -.8;
				OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);

				viewDef.lights.numFillLights 		= 1;
				viewDef.lights.ambientColor.r 		= .4;
				viewDef.lights.ambientColor.g 		= .4;
				viewDef.lights.ambientColor.b 		= .3;
				viewDef.lights.fillDirection[0] 	= gWorldSunDirection;
				viewDef.lights.fillColor[0] 		= gFillColor1;
				break;

		case	LEVEL_NUM_PLAYROOM:
				gWorldSunDirection.x = .5;
				gWorldSunDirection.y = -.8;
				gWorldSunDirection.z = -.3;
				OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);

				viewDef.lights.numFillLights 		= 2;
				viewDef.lights.ambientColor.r 		= .35;
				viewDef.lights.ambientColor.g 		= .35;
				viewDef.lights.ambientColor.b 		= .30;

				viewDef.lights.fillDirection[0].x 	= 1;
				viewDef.lights.fillDirection[0].y 	= -.4;
				viewDef.lights.fillDirection[0].z 	= 1;
				viewDef.lights.fillColor[0].r 		= 1.0;
				viewDef.lights.fillColor[0].g 		= .7;
				viewDef.lights.fillColor[0].b 		= .7;

				viewDef.lights.fillDirection[1].x 	= -1;
				viewDef.lights.fillDirection[1].y 	= -.2;
				viewDef.lights.fillDirection[1].z 	= -.5;
				viewDef.lights.fillColor[1].r 		= .7;
				viewDef.lights.fillColor[1].g 		= .7;
				viewDef.lights.fillColor[1].b 		= 1.0;
				break;

		case	LEVEL_NUM_CLOSET:
				gWorldSunDirection.x = .3;
				gWorldSunDirection.y = -.6;
				gWorldSunDirection.z = -.5;
				OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);

				viewDef.lights.numFillLights 		= 1;
				viewDef.lights.ambientColor.r 		= .2;
				viewDef.lights.ambientColor.g 		= .2;
				viewDef.lights.ambientColor.b 		= .3;
				viewDef.lights.fillDirection[0] 	= gWorldSunDirection;
				viewDef.lights.fillColor[0].r 		= .25;
				viewDef.lights.fillColor[0].g 		= .25;
				viewDef.lights.fillColor[0].b 		= .4;
				break;

		case	LEVEL_NUM_GARBAGE:
				gWorldSunDirection.x = -.5;
				gWorldSunDirection.y = -.5;
				gWorldSunDirection.z = -.8;
				OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);

				viewDef.lights.numFillLights 		= 1;
				viewDef.lights.ambientColor.r 		= .3;
				viewDef.lights.ambientColor.g 		= .3;
				viewDef.lights.ambientColor.b 		= .3;
				viewDef.lights.fillDirection[0] 	= gWorldSunDirection;
				viewDef.lights.fillColor[0].r 		= .8;
				viewDef.lights.fillColor[0].g 		= .9;
				viewDef.lights.fillColor[0].b 		= .8;
				break;


		case	LEVEL_NUM_BALSA:
				gWorldSunDirection.x = .8;
				gWorldSunDirection.y = -.6;
				gWorldSunDirection.z = -.9;
				OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);

				viewDef.lights.numFillLights 		= 1;
				viewDef.lights.ambientColor.r 		= .3;
				viewDef.lights.ambientColor.g 		= .3;
				viewDef.lights.ambientColor.b 		= .3;
				viewDef.lights.fillDirection[0] 	= gWorldSunDirection;
				viewDef.lights.fillColor[0].r 		= 1.0;
				viewDef.lights.fillColor[0].g 		= 1.0;
				viewDef.lights.fillColor[0].b 		= .9;
				break;


		case	LEVEL_NUM_PARK:
				gWorldSunDirection.x = .8;
				gWorldSunDirection.y = -.35;
				gWorldSunDirection.z = -.2;
				OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);

				viewDef.lights.numFillLights 		= 1;
				viewDef.lights.ambientColor.r 		= .4;
				viewDef.lights.ambientColor.g 		= .4;
				viewDef.lights.ambientColor.b 		= .3;
				viewDef.lights.fillDirection[0] 	= gWorldSunDirection;
				viewDef.lights.fillColor[0].r 		= .9;
				viewDef.lights.fillColor[0].g 		= .9;
				viewDef.lights.fillColor[0].b 		= .8;
				break;



		default:
				gWorldSunDirection.x = .4;
				gWorldSunDirection.y = -.5;
				gWorldSunDirection.z = -.8;
				OGLVector3D_Normalize(&gWorldSunDirection,&gWorldSunDirection);

				viewDef.lights.numFillLights 		= 1;
				viewDef.lights.ambientColor.r 		= .5;
				viewDef.lights.ambientColor.g 		= .5;
				viewDef.lights.ambientColor.b 		= .4;
				viewDef.lights.fillDirection[0] 	= gWorldSunDirection;
				viewDef.lights.fillColor[0] 		= gFillColor1;
	}



	OGL_SetupWindow(&viewDef, &gGameViewInfoPtr);


			/**********************/
			/* SET AUTO-FADE INFO */
			/**********************/

	switch(gLevelNum)
	{
		case	LEVEL_NUM_FIDO:
		case	LEVEL_NUM_BALSA:
		case	LEVEL_NUM_CLOSET:
				gAutoFadeStartDist	= 0;				// no auto-fading here
				break;

		default:
				gAutoFadeStartDist	= gGameViewInfoPtr->yon * .80;
				gAutoFadeEndDist	= gGameViewInfoPtr->yon * .95f;
	}


	if (!gG4)												// bring it in a little for slow machines
	{
		gAutoFadeStartDist *= .85f;
		gAutoFadeEndDist *= .85f;
	}

	gAutoFadeRange_Frac	= 1.0f / (gAutoFadeEndDist - gAutoFadeStartDist);

	if (gAutoFadeStartDist != 0.0f)
		gAutoFadeStatusBits = STATUS_BIT_AUTOFADE;
	else
		gAutoFadeStatusBits = 0;



			/**********************/
			/* LOAD ART & TERRAIN */
			/**********************/
			//
			// NOTE: only call this *after* draw context is created!
			//

	LoadLevelArt_Explore(gGameViewInfoPtr);
	InitInfobar(gGameViewInfoPtr);

			/* INIT OTHER MANAGERS */

	InitEnemyManager();
	InitEffects(gGameViewInfoPtr);
	InitVaporTrails();
	InitSparkles();
	InitItemsManager();
	InitDialogManager();


			/****************/
			/* INIT SPECIAL */
			/****************/

	gGravity = NORMAL_GRAVITY;					// assume normal gravity

	switch(gLevelNum)
	{
		case	LEVEL_NUM_SIDEWALK:
				InitSnakeStuff();
				CountSquishBerries();
				break;

		case	LEVEL_NUM_FIDO:
				CountTicks();
				CountFleas();
				DoDialogMessage(DIALOG_MESSAGE_REMEMBERDOG, 1, 11.0, nil);
				break;

		case	LEVEL_NUM_PLAYROOM:
				InitSlotCarRacing();
				break;

		case	LEVEL_NUM_GARBAGE:
				gNumDrowningMiceRescued = 0;				// no drowning mice rescued yet
				break;

		case	LEVEL_NUM_BALSA:
				DoDialogMessage(DIALOG_MESSAGE_BOMBHILLS, 0, 10.0, nil);
				DoDialogMessage(DIALOG_MESSAGE_BOMBHILLS2, 1, 6.0, nil);
				break;

		case	LEVEL_NUM_PARK:
				gNumFoodOnBasket = 0;
				gNumCaughtFish = 0;
				gKindlingCount = 0;
				gBurnKindling = false;
				gIgnoreBottleKeySnail = false;
				break;

	}

		/* INIT THE PLAYER & RELATED STUFF */

	PrimeTerrainWater();							// NOTE:  must do this before items get added since some items may be on the water
	InitPlayerAtStartOfLevel_Terrain();				// NOTE:  this will also cause the initial items in the start area to be created

	PrimeSplines();
	PrimeFences();

			/* INIT CAMERAS */

	InitCamera_Terrain();


 }


/***************** START LEVEL COMPLETION ****************/

void StartLevelCompletion(float coolDownTimer)
{
	if (!gLevelCompleted)
	{
		gLevelCompleted = true;
		gLevelCompletedCoolDownTimer = coolDownTimer;
	}
}

/**************** CLEANUP LEVEL **********************/

static void CleanupLevel(void)
{

	StopAllEffectChannels();
	DisposeTunnelData();
 	EmptySplineObjectList();
	DeleteAllObjects();
	FreeAllSkeletonFiles(-1);
	DisposeTerrain();
	DeleteAllParticleGroups();
	DeleteAllConfettiGroups();
	DisposeInfobar();
	DisposeParticleSystem();
	DisposeAllSpriteGroups();

	DisposeAllBG3DContainers();



	DisposeSoundBank(SOUND_BANK_LEVELSPECIFIC);

	OGL_DisposeWindowSetup(&gGameViewInfoPtr);	// do this last!
}



#pragma mark -

/************************************************************/
/******************** PROGRAM MAIN ENTRY  *******************/
/************************************************************/


int main(void)
{
unsigned long	someLong;


				/**************/
				/* BOOT STUFF */
				/**************/

	ToolBoxInit();




			/* INIT SOME OF MY STUFF */

	InitSpriteManager();
	InitBG3DManager();
	InitWindowStuff();
	InitTerrainManager();
	InitSkeletonManager();
	InitSoundTools();


			/* INIT MORE MY STUFF */

	InitObjectManager();

	GetDateTime ((unsigned long *)(&someLong));		// init random seed
	SetMyRandomSeed(someLong);
	HideRealCursor();



		/* SEE IF DEMO EXPIRED */

	if (DEMO || (!gGameIsRegistered))
		GetDemoTimer();


		/* SHOW TITLES */

	DoLegalScreen();
	DoTitleScreen();




		/* MAIN LOOP */

	while(true)
	{
		MyFlushEvents();
		DoMainMenuScreen();

		PlayGame();
	}

}



