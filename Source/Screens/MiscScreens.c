/****************************/
/*   	MISCSCREENS.C	    */
/* (c)2002 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "3dmath.h"
#include "internet.h"

extern	float				gFramesPerSecondFrac,gFramesPerSecond;
extern	FSSpec		gDataSpec;
extern	KeyMap gKeyMap,gNewKeys;
extern	NewObjectDefinitionType	gNewObjectDefinition;
extern	Boolean		gDisableAnimSounds;
extern	PrefsType	gGamePrefs;
extern	OGLPoint3D	gCoord;
extern	int			gLevelNum;
extern	OGLSetupOutputType		*gGameViewInfoPtr;
extern	OGLVector3D	gDelta;
extern	GDHandle				gOurDevice;
extern	float					gAnaglyphScaleFactor, gAnaglyphFocallength, gAnaglyphEyeSeparation;


/****************************/
/*    PROTOTYPES            */
/****************************/

static void DisplayPicture_Draw(OGLSetupOutputType *info);
static pascal OSStatus DoGameSettingsDialog_EventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData);


/****************************/
/*    CONSTANTS             */
/****************************/



/*********************/
/*    VARIABLES      */
/*********************/

MOPictureObject 	*gBackgoundPicture = nil;

OGLSetupOutputType	*gScreenViewInfoPtr = nil;

static Boolean		gLanguageChanged = false;


/********************** DISPLAY PICTURE **************************/
//
// Displays a picture using OpenGL until the user clicks the mouse or presses any key.
// If showAndBail == true, then show it and bail out
//

void DisplayPicture(FSSpec *spec)
{
OGLSetupInputType	viewDef;
float	timeout = 40.0f;



			/* SETUP VIEW */

	OGL_NewViewDef(&viewDef);

	viewDef.camera.hither 			= 10;
	viewDef.camera.yon 				= 3000;
	viewDef.view.clearColor.r 		= 0;
	viewDef.view.clearColor.g 		= 0;
	viewDef.view.clearColor.b		= 0;
	viewDef.styles.useFog			= false;

	OGL_SetupWindow(&viewDef, &gGameViewInfoPtr);



			/* CREATE BACKGROUND OBJECT */

	gBackgoundPicture = MO_CreateNewObjectOfType(MO_TYPE_PICTURE, (u_long)gGameViewInfoPtr, spec);
	if (!gBackgoundPicture)
		DoFatalAlert("\pDisplayPicture: MO_CreateNewObjectOfType failed");



		/***********/
		/* SHOW IT */
		/***********/


	MakeFadeEvent(true, 1);
	UpdateInput();
	CalcFramesPerSecond();

					/* MAIN LOOP */

		while(!Button())
		{
			CalcFramesPerSecond();
			MoveObjects();
			OGL_DrawScene(gGameViewInfoPtr, DisplayPicture_Draw);

			UpdateInput();
			if (AreAnyNewKeysPressed())
				break;

			timeout -= gFramesPerSecondFrac;
			if (timeout < 0.0f)
				break;
		}


			/* CLEANUP */

	DeleteAllObjects();
	MO_DisposeObjectReference(gBackgoundPicture);
	DisposeAllSpriteGroups();


			/* FADE OUT */

	GammaFadeOut();


	OGL_DisposeWindowSetup(&gGameViewInfoPtr);


}


/***************** DISPLAY PICTURE: DRAW *******************/

static void DisplayPicture_Draw(OGLSetupOutputType *info)
{
	MO_DrawObject(gBackgoundPicture, info);
	DrawObjects(info);
}


#pragma mark -

/************** DO LEGAL SCREEN *********************/

void DoLegalScreen(void)
{
FSSpec	spec;

	GammaFadeOut();

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, "\p:Images:Pangea", &spec);

	DisplayPicture(&spec);

}



#pragma mark -


/****************** DO DEMO EXPIRED SCREEN **************************/

void DoDemoExpiredScreen(void)
{
FSSpec	spec;

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, "\p:Images:DemoExpired", &spec);
	DisplayPicture(&spec);

	LaunchURL("\phttp://www.pangeasoft.net/bug2/buy.html");

	CleanQuit();
}


/*************** SHOW DEMO QUIT SCREEN **********************/

void ShowDemoQuitScreen(void)
{
FSSpec	spec;

	SaveDemoTimer();						// make sure to save this before we bail

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, "\p:Images:DemoQuit", &spec);
	DisplayPicture(&spec);

	if (IsInternetAvailable())				// if we've got TCP connection then launch URL
	{
		CleanupDisplay();								// unloads Draw Sprocket
		LaunchURL("\phttp://www.pangeasoft.net/bug2/buy.html");
	}
}



/********************* DO LEVEL CHEAT DIALOG **********************/

#if 0
Boolean DoLevelCheatDialog(void)
{
DialogPtr 		myDialog;
short			itemHit;
Boolean			dialogDone = false;

	Enter2D();

	ShowRealCursor();
	myDialog = GetNewDialog(132,nil,MOVE_TO_FRONT);
	if (!myDialog)
	{
		DoAlert("\pDoLevelCheatDialog: GetNewDialog failed!");
		ShowSystemErr(ResError());
	}

	AutoSizeDialog(myDialog);



	GammaFadeIn();


				/* DO IT */

	MyFlushEvents();
	while(!dialogDone)
	{
		ModalDialog(nil, &itemHit);
		switch (itemHit)
		{
			case 	1:									// hit Quit
					CleanQuit();

			default:									// selected a level
					gLevelNum = (itemHit - 2);
					dialogDone = true;
		}
	}
	DisposeDialog(myDialog);
	HideRealCursor();
	GammaFadeOut();
	GameScreenToBlack();


	Exit2D();

	return(false);
}
#endif

#pragma mark -

/********************** DO GAME SETTINGS DIALOG ************************/

void DoGameOptionsDialog(void)
{
OSErr			err;
EventTypeSpec	list[] = { { kEventClassCommand,  kEventProcessCommand } };
WindowRef 		dialogWindow = nil;
EventHandlerUPP winEvtHandler;
ControlID 		idControl;
ControlRef 		control;
EventHandlerRef	ref;

const char		*rezNames[MAX_LANGUAGES] =
{
	"Settings_English",
	"Settings_French",
	"Settings_German",
	"Settings_Spanish",
	"Settings_Italian",
	"Settings_Swedish",
	"Settings_Dutch",
};


	if (gGamePrefs.language >= MAX_LANGUAGES)		// verify prefs for the hell of it.
		InitDefaultPrefs();

	Enter2D();
	ShowRealCursor();
//	MyFlushEvents();


    		/***************/
    		/* INIT DIALOG */
    		/***************/

do_again:

	gLanguageChanged  = false;

			/* CREATE WINDOW FROM THE NIB */

    err = CreateWindowFromNib(gNibs,CFStringCreateWithCString(nil, rezNames[gGamePrefs.language],
    						kCFStringEncodingMacRoman), &dialogWindow);
	if (err)
		DoFatalAlert("\pDoGameOptionsDialog: CreateWindowFromNib failed!");

			/* CREATE NEW WINDOW EVENT HANDLER */

    winEvtHandler = NewEventHandlerUPP(DoGameSettingsDialog_EventHandler);
    InstallWindowEventHandler(dialogWindow, winEvtHandler, GetEventTypeCount(list), list, dialogWindow, &ref);


			/* SET "SHOW VIDEO" CHECKBOX */

    idControl.signature = 'vido';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	SetControlValue(control, gGamePrefs.showScreenModeDialog);


			/* SET "KIDDIE MODE" CHECKBOX */

    idControl.signature = 'kidm';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	SetControlValue(control, gGamePrefs.kiddieMode);


			/* SET LANGUAGE  */

    idControl.signature = 'lang';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	SetControlValue(control, gGamePrefs.language + 1);



			/**********************/
			/* PROCESS THE DIALOG */
			/**********************/

    ShowWindow(dialogWindow);
	RunAppModalLoopForWindow(dialogWindow);


			/*********************/
			/* GET RESULT VALUES */
			/*********************/

			/* GET "SHOW VIDEO" CHECKBOX */

    idControl.signature = 'vido';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	gGamePrefs.showScreenModeDialog = GetControlValue(control);


			/* GET "KIDDIE MODE" CHECKBOX */

    idControl.signature = 'kidm';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	gGamePrefs.kiddieMode = GetControlValue(control);



			/* GET LANGUAGE */

    idControl.signature = 'lang';
    idControl.id 		= 0;
    GetControlByID(dialogWindow, &idControl, &control);
	gGamePrefs.language = GetControlValue(control) - 1;


				/***********/
				/* CLEANUP */
				/***********/

	DisposeEventHandlerUPP (winEvtHandler);
	DisposeWindow (dialogWindow);

			/* IF IT WAS JUST A LANGUAGE CHANGE THEN GO BACK TO THE DIALOG */

	if (gLanguageChanged)
		goto do_again;

	HideRealCursor();
	Exit2D();
	SavePrefs();

	CalcFramesPerSecond();				// reset this so things dont go crazy when we return
	CalcFramesPerSecond();

}


/****************** DO GAME SETTINGS DIALOG EVENT HANDLER *************************/

static pascal OSStatus DoGameSettingsDialog_EventHandler(EventHandlerCallRef myHandler, EventRef event, void* userData)
{
#pragma unused (myHandler, userData)
OSStatus			result = eventNotHandledErr;
HICommand 			command;

	switch(GetEventKind(event))
	{

				/*******************/
				/* PROCESS COMMAND */
				/*******************/

		case	kEventProcessCommand:
				GetEventParameter (event, kEventParamDirectObject, kEventParamHICommand, NULL, sizeof(command), NULL, &command);
				switch(command.commandID)
				{
							/* OK BUTTON */

					case	kHICommandOK:
		                    QuitAppModalLoopForWindow((WindowRef) userData);
		                    break;


							/* LANGUAGE */

					case	'lang':
							gLanguageChanged = true;
		                    QuitAppModalLoopForWindow((WindowRef) userData);
							break;


							/* CLEAR HIGH SCORES */

					case	'clhs':
							ClearHighScores();
							break;


							/* CONFIGURE INPUT */

					case	'cinp':
							DoInputConfigDialog();
							break;

				}
				break;
    }

    return (result);
}



#pragma mark -


/*********************** DO LEVEL INTRO ***************************/

void DoLevelIntro(void)
{
			/* SET ANAGLYPH INFO */

	if (gGamePrefs.anaglyph)
	{
		gAnaglyphScaleFactor 	= 1.0f;
		gAnaglyphFocallength	= 200.0f * gAnaglyphScaleFactor;	// set camera info
		gAnaglyphEyeSeparation 	= 25.0f * gAnaglyphScaleFactor;
	}

	switch(gLevelNum)
	{
		case	LEVEL_NUM_GNOMEGARDEN:
				DoLevelIntroScreen_FrontYard();
				break;

		case	LEVEL_NUM_SIDEWALK:
				DoLevelIntroScreen_BackYard();
				break;

		case	LEVEL_NUM_PLUMBING:
				DoLevelIntroScreen_Sewer();
				break;

		case	LEVEL_NUM_PLAYROOM:
				DoLevelIntroScreen_Playroom();
				break;

		case	LEVEL_NUM_CLOSET:
				DoLevelIntroScreen_Closet();
				break;

		case	LEVEL_NUM_BALSA:
				DoLevelIntroScreen_Balsa();
				break;

		case	LEVEL_NUM_FIDO:
				DoLevelIntroScreen_Fido();
				break;

		case	LEVEL_NUM_GUTTER:
				DoLevelIntroScreen_Gutter();
				break;

		case	LEVEL_NUM_GARBAGE:
				DoLevelIntroScreen_Garbage();
				break;

		case	LEVEL_NUM_PARK:
				DoLevelIntroScreen_Park();
				break;
	}

}





