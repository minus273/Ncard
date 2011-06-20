/****************************************************************************
 * libwiigui Template
 * Tantric 2009
 *
 * menu.cpp
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <gccore.h>
#include <ogcsys.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>


#include "libwiigui/gui.h"
#include "menu.h"
#include "demo.h"
#include "input.h"
#include "filelist.h"
#include "filebrowser.h"


#include "http.h"
#include "ncard.h"
#include "getstats.h"

#define THREAD_SLEEP 100

static GuiImageData * pointer[4];
static GuiImage * bgImg = NULL;
static GuiSound * bgMusic = NULL;
static GuiWindow * mainWindow = NULL;
static lwp_t guithread = LWP_THREAD_NULL;
static bool guiHalt = true;
char key[32];

char username[32];

/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
static void
ResumeGui()
{
	guiHalt = false;
	LWP_ResumeThread (guithread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
static void
HaltGui()
{
	guiHalt = true;

	// wait for thread to finish
	while(!LWP_ThreadIsSuspended(guithread))
		usleep(THREAD_SLEEP);
}

/****************************************************************************
 * WindowPrompt
 *
 * Displays a prompt window to user, with information, an error message, or
 * presenting a user with a choice
 ***************************************************************************/
int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){0, 0, 0, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,40);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetWrap(true, 400);

	GuiText btn1Txt(btn1Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(20, -25);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -25);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetTrigger(&trigA);
	btn1.SetState(STATE_SELECTED);
	btn1.SetEffectGrow();

	GuiText btn2Txt(btn2Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-20, -25);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetSoundOver(&btnSoundOver);
	btn2.SetTrigger(&trigA);
	btn2.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	if(btn2Label)
		promptWindow.Append(&btn2);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&promptWindow);
	mainWindow->ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		usleep(THREAD_SLEEP);

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(THREAD_SLEEP);
	HaltGui();
	mainWindow->Remove(&promptWindow);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}


/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/

static void *
UpdateGUI (void *arg)
{
	int i;

	while(1)
	{
		if(guiHalt)
		{
			LWP_SuspendThread(guithread);
		}
		else
		{
			UpdatePads();
			mainWindow->Draw();

			#ifdef HW_RVL
			for(i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad->ir.valid)
					Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
						96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
				DoRumble(i);
			}
			#endif

			Menu_Render();

			for(i=0; i < 4; i++)
				mainWindow->Update(&userInput[i]);

			if(ExitRequested)
			{
				for(i = 0; i < 255; i += 15)
				{
					mainWindow->Draw();
					Menu_DrawRectangle(0,0,screenwidth,screenheight,(GXColor){0, 0, 0, i},1);
					Menu_Render();
				}
				ExitApp();
			}
		}
	}
	return NULL;
}

/****************************************************************************
 * InitGUIThread
 *
 * Startup GUI threads
 ***************************************************************************/
void
InitGUIThreads()
{
	LWP_CreateThread (&guithread, UpdateGUI, NULL, NULL, 0, 70);
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, u16 maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWindow->SetState(STATE_DISABLED);
	mainWindow->Append(&keyboard);
	mainWindow->ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		usleep(THREAD_SLEEP);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWindow->Remove(&keyboard);
	mainWindow->SetState(STATE_DEFAULT);
	ResumeGui();
}


/****************************************************************************
 * MenuSettings
 ***************************************************************************/
void progress(int record, int total)
{

}

static int MenuSettings()
{

	 int err; 
	int menu = MENU_NONE;

	GuiText titleTxt("NCard Client .04", 28, (GXColor){255, 255, 255, 255});
	titleTxt.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	titleTxt.SetPosition(50,30);
	
	
	u8 *card_png; //define the variables to hold your image file
	  u32 card_png_size=0;
	size_t amount_read;
	FILE *fp = fopen("sd:/apps/ncard/card.png","r"); //open the png file
	if(fp)  //make sure the file exists
	{
	fseek (fp , 0 , SEEK_END);
	card_png_size = ftell(fp); //find the file size
	rewind(fp);
	
	card_png = new u8 [card_png_size]; //allocate memory for your image buffer
	if(card_png) //make sure memory allocated
	{
	amount_read = fread(card_png,1,card_png_size,fp); //read file to buffer
	}
	
	
		
		
	fclose(fp); //close file
	}
	else
	{
	/*	int err =WindowPrompt(
					 "Error:",
					 "Could not load NCard Image",
					 "Close",
					 0);
		 
	*/
	 
	
	int init=Ncard_init();
	
	if(init<0)
	{
		 err =WindowPrompt(
							"Error:",
							"Could not initilize network",
							"Close",
							0);
		
	}
	
	struct block file_data ; 
	
	char url[1024], buf[1024];
	sprintf(url,"http://www.messageboardchampion.com/ncard/cache/%s.png",username);
	encode_url(buf,url);
	
		/*WindowPrompt(
					 "Error:",
					 username,
					 "Close",
					 0);

		*/
		
		
	
	file_data=downloadfile(buf);
		card_png=file_data.data;
	
	
	if(fp=fopen("sd:/apps/ncard/card.png","w"))
	{
		fwrite(file_data.data,file_data.size,1,fp);
		fclose(fp);
	}
	   
	}
	
	
	GuiImageData theCard(card_png);
	GuiImage myCard(&theCard); 
	myCard.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	myCard.SetPosition(270, 80);
	

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData btnLargeOutline(button_large_png);
	GuiImageData btnLargeOutlineOver(button_large_over_png);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	GuiTrigger trigHome;
	trigHome.SetButtonOnlyTrigger(-1, WPAD_BUTTON_HOME | WPAD_CLASSIC_BUTTON_HOME, 0);

	GuiText syncBtnTxt("Sync NCard", 22, (GXColor){0, 0, 0, 255});
	syncBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage syncBtnImg(&btnLargeOutline);
	GuiImage syncBtnImgOver(&btnLargeOutlineOver);
	GuiButton syncBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	syncBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	syncBtn.SetPosition(20, 80);
	syncBtn.SetLabel(&syncBtnTxt);
	syncBtn.SetImage(&syncBtnImg);
	syncBtn.SetImageOver(&syncBtnImgOver);
	syncBtn.SetSoundOver(&btnSoundOver);
	syncBtn.SetTrigger(&trigA);
	syncBtn.SetEffectGrow();
	
	GuiText codeBtnTxt("Bind Wii Code", 22, (GXColor){0, 0, 0, 255});
	codeBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage codeBtnImg(&btnLargeOutline);
	GuiImage codeBtnImgOver(&btnLargeOutlineOver);
	GuiButton codeBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	codeBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	codeBtn.SetPosition(20, 180);
	codeBtn.SetLabel(&codeBtnTxt);
	codeBtn.SetImage(&codeBtnImg);
	codeBtn.SetImageOver(&codeBtnImgOver);
	codeBtn.SetSoundOver(&btnSoundOver);
	codeBtn.SetTrigger(&trigA);
	codeBtn.SetEffectGrow();

	GuiText aboutBtnTxt("About", 22, (GXColor){0, 0, 0, 255});
	aboutBtnTxt.SetWrap(true, btnLargeOutline.GetWidth()-30);
	GuiImage aboutBtnImg(&btnLargeOutline);
	GuiImage aboutBtnImgOver(&btnLargeOutlineOver);
	GuiButton aboutBtn(btnLargeOutline.GetWidth(), btnLargeOutline.GetHeight());
	aboutBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	aboutBtn.SetPosition(20, 280);
	aboutBtn.SetLabel(&aboutBtnTxt);
	aboutBtn.SetImage(&aboutBtnImg);
	aboutBtn.SetImageOver(&aboutBtnImgOver);
	aboutBtn.SetSoundOver(&btnSoundOver);
	aboutBtn.SetTrigger(&trigA);
	aboutBtn.SetEffectGrow();

	GuiText exitBtnTxt("Exit", 22, (GXColor){0, 0, 0, 255});
	GuiImage exitBtnImg(&btnOutline);
	GuiImage exitBtnImgOver(&btnOutlineOver);
	GuiButton exitBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	exitBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	exitBtn.SetPosition(100, -35);
	exitBtn.SetLabel(&exitBtnTxt);
	exitBtn.SetImage(&exitBtnImg);
	exitBtn.SetImageOver(&exitBtnImgOver);
	exitBtn.SetSoundOver(&btnSoundOver);
	exitBtn.SetTrigger(&trigA);
	exitBtn.SetTrigger(&trigHome);
	exitBtn.SetEffectGrow();

	GuiText resetBtnTxt("Reset Settings", 22, (GXColor){0, 0, 0, 255});
	GuiImage resetBtnImg(&btnOutline);
	GuiImage resetBtnImgOver(&btnOutlineOver);
	GuiButton resetBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	resetBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	resetBtn.SetPosition(-100, -35);
	resetBtn.SetLabel(&resetBtnTxt);
	resetBtn.SetImage(&resetBtnImg);
	resetBtn.SetImageOver(&resetBtnImgOver);
	resetBtn.SetSoundOver(&btnSoundOver);
	resetBtn.SetTrigger(&trigA);
	resetBtn.SetEffectGrow();

	HaltGui();
	GuiWindow w(screenwidth, screenheight);
	w.Append(&titleTxt);
	w.Append(&codeBtn);
	w.Append(&syncBtn);
	w.Append(&aboutBtn);
	w.Append(&myCard); 

	w.Append(&exitBtn);
	w.Append(&resetBtn);

	mainWindow->Append(&w);

	ResumeGui();

	while(menu == MENU_NONE)
	{
		usleep(THREAD_SLEEP);

		if(codeBtn.GetState() == STATE_CLICKED)
		{
			// set wii code
			menu = MENU_CODE; // refreshes view
			
			
			int init=Ncard_init();
			
			if(init<0)
			{
				err =WindowPrompt(
								  "Error:",
								  "Could not initilize network",
								  "Close",
								  0);
				
			}
			else
				
			{
				
				
				char friendcode[20]; 
				err=getfriendcode(friendcode); 
				
				if(Ncard_friendcode(key, friendcode)==0)
				{
					err =WindowPrompt(
									  "Success",
									  "Code submitted. Refreshing. ",
									  "Close",
									  0);
					int rem2=remove("sd:/apps/ncard/card.png"); // force card to refresh
				}
				else
				{
					err =WindowPrompt(
									  "Error:",
									  "Could not submit Wii Code",
									  "Close",
									  0);
				}
				
			}
			
		}
		else if(aboutBtn.GetState() == STATE_CLICKED)
		{
			int choice = WindowPrompt(
									  "NCard Developed By:",
									  "Chris: chris@toxicbreakfast.com & Minus_273: minus_273@messageboardchampion.com",
									  "Close",
									  0);
			
		}
		else if(syncBtn.GetState() == STATE_CLICKED)
		{
			// sync games
			menu = MENU_SYNC; //refreh view
			
			int init=Ncard_init();
			
			if(init<0)
			{
				err =WindowPrompt(
								  "Error:",
								  "Could not initilize network",
								  "Close",
								  0);
				
			}
			else
			{
				stats games;
				err=getstats(games,0,"sd:/apps/ncard/filters.txt",&progress);
				if(err==0)
				{
					/*printf("\n\n\x1b[37;1m%sLast 10 games played:\n",margin);
					for(i=0;i<STATS_LENGTH;i++)
					{
						utf8_to_ascii(games[i].title,buffer);
						//strcpy(buffer,games[i].title_id);
						printf("\x1b[33;1m    %02d: \x1b[36;1m%-20s",i+1,buffer);
						if(i%2==1)
							printf("\n");
					}*/
				}
				else if(err==-1)
				{
					err =WindowPrompt(
									  "Error:",
									  "Could not read Wii messageboard",
									  "Close",
									  0);
				}
				
				if(strlen(games[0].title)>0)
				{
					//printf("\x1b[37;1m%sSubmitting...",margin);
					char* response=Ncard_submit_multiple(1,key,10,games[9].title_id,games[8].title_id,games[7].title_id,games[6].title_id,games[5].title_id,games[4].title_id,games[3].title_id,games[2].title_id,games[1].title_id,games[0].title_id);
					if(response!=NULL)
					{
						/*printf("\x1b[31;1mFailed:\n%s\x1b[37;1m%s",margin,response);
						sleep(5);
						exit(1);*/
						
						err =WindowPrompt(
										  "Warning:",
										  "Card has updated but some games failed to submit.",
										  "Close",
										  0);
					}
					else
					{
						err =WindowPrompt(
										  "Success",
										  "Card has been updated! Refreshing. ",
										  "Close",
										  0);
					int rem2=remove("sd:/apps/ncard/card.png"); // force card to refresh 
					}
				}
				else
					err =WindowPrompt(
									  "Error:",
									  "No games to submit",
									  "Close",
									  0);
				
				
			}
			
			
		}
		else if(exitBtn.GetState() == STATE_CLICKED)
		{
			menu = MENU_EXIT;
		}
		else if(resetBtn.GetState() == STATE_CLICKED)
		{
			resetBtn.ResetState();

			int choice = WindowPrompt(
				"Reset Settings",
				"Are you sure that you want to reset your settings?",
				"Yes",
				"No");
			if(choice == 1)
			{
				// delete ncard dat file
				int rem1=remove("sd:/apps/ncard/ncard.dat"); 
				int rem3=remove("sd:/apps/ncard/ncard1.dat"); 
				//delete card cache
				int rem2=remove("sd:/apps/ncard/card.png");
				
				
				if((rem1!=0) || (rem2!=0) || (rem3!=0))
				{
					WindowPrompt(
								 "Error:",
								 "Settings files were not removed. Delete dat files in the NCard folder manually.  ",
								 "Next",
								 0);
				}
				else
				{
					WindowPrompt(
								 "Success",
								 "Settings files were removed. ",
								 "Next",
								 0);
				}
				
				WindowPrompt(
							 "Re-Login",
							 "Reload NCard to log back in.",
							 "Close",
							 0);
				
				menu=MENU_EXIT; 
				
			}
		}
	}

	HaltGui();
	mainWindow->Remove(&w);
	return menu;
}



/****************************************************************************
 * MainMenu
 ***************************************************************************/

int loginProcess(char* user, char* pass)
{
	int init=Ncard_init();
	FILE* fp;
	int menu; 
	
	WindowPrompt(
				 "Ready",
				 "We are ready to login.",
				 "Login!",
				 0);
	
	if(init<0)
	{
		WindowPrompt(
					 "Error:",
					 "Could not initilize network",
					 "Close",
					 0);
		menu=MENU_EXIT;  
		
	}
	else
	{
		
		char* response=Ncard_key(key,user,pass);
		if(response==NULL)
		{
			
			if(fp=fopen("sd:/apps/ncard/ncard.dat","w"))
			{
				fwrite(key,32,1,fp);
				fclose(fp);
			}
			else
			{
				WindowPrompt(
							 "Error:",
							 "Login was ok. Problem saving the key. ",
							 "Close",
							 0);
				menu=MENU_EXIT; 
			}
		} else
			
		{
			WindowPrompt(
						 "Error:",
						 "Invalid username or password",
						 "Close",
						 0);
			menu=MENU_EXIT; 
		}
	}
	return menu; 
}

void MainMenu(int menu)
{
	int currentMenu = menu;

	


	
	#ifdef HW_RVL
	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);
	#endif

	mainWindow = new GuiWindow(screenwidth, screenheight);

	bgImg = new GuiImage(screenwidth, screenheight, (GXColor){50, 50, 50, 255});
	bgImg->ColorStripe(30);
	mainWindow->Append(bgImg);

	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	ResumeGui();

	int init= Ncard_init();// just to make sure.. 
	
	while(init<0)
	{
		
		WindowPrompt(
					 "Error:",
					 "Could not initilize network",
					 "Try again",
					 0);
		init= Ncard_init();
	
	}
//	} else {

		/*WindowPrompt(
					 "Success:",
					 "Got Network connection",
					 "Close",
					 0);
		*/
	//}
	
	
	FILE* fp; 
	int amt1=0, amt=0; 
	
	if((fp=fopen("sd:/apps/ncard/ncard.dat","r")))
	{
		fread(key,32,1,fp);
		key[32]=0;
		fclose(fp);
		amt=1; //set flag
	
	}
	
	if((fp=fopen("sd:/apps/ncard/ncard1.dat","r")))
	{
		fread(username,32,1,fp);
		//username[amt1]=0;
		fclose(fp);
		amt1=1; //set flag
		
	}
	
	if(amt1>0&&amt>0)
	{
	}
	else
	{
		Ncard_init();
		
		int choice=WindowPrompt(
					 "Welcome to the NCard!",
					 "You need an account on messageboardchampion.com to  use this. ",
					 "I have one",
					 "I do not have one");
		if(choice==1)
		{
		
		
		WindowPrompt(
					 "Username",
					 "Please enter your username ",
					 "Enter Username",
					 0);
		
		//read username and password
		char user[32]; 
		char pass[32];
		
			user[0]='\0';
			pass[0]='\0';
			
		
		OnScreenKeyboard(user,32); 
			
			sprintf(username, "%s", user); 
		
		WindowPrompt(
					 "Password",
					 "Please enter your password ",
					 "Enter Password",
					 0);
		OnScreenKeyboard(pass,32); 
			
		// save username in datfile	
			if(fp=fopen("sd:/apps/ncard/ncard1.dat","w"))
			{
				fwrite(user,strlen(user),1,fp);
				fclose(fp);
			}
			
		// call ncard to get  key
			
			
			currentMenu=loginProcess(user, pass); 
		
			
			
		}
		else
		{
			char user[32]; 
			char pass[32];
			
			user[0]='\0';
			pass[0]='\0';
			
			Ncard_init();
			
			WindowPrompt(
						 "Create an Account",
						 "We will now create an account on Messageboardchampion.com",
						 "Next",
						 0);
			//username
			
			char* response=""; 
			
			while(response!=NULL)
			{
			WindowPrompt(
						 "Username",
						 "Please enter your desired username ",
						 "Enter Username",
						 0);
			
			//read username and password
			
			
			OnScreenKeyboard(user,32); 
		
			
			// check username
			response=Ncard_checkUser(user);
				if(response!=NULL)
				{
					WindowPrompt(
								 "Error:",
								 "That username is already in use. ",
								 "Retry",
								 0);
				}
			}
			
			WindowPrompt(
						 "Success",
						 "This username can be used",
						 "Next",
						 0);
			
			sprintf(username, "%s", user); 
			
			WindowPrompt(
						 "Password",
						 "Please enter your password ",
						 "Enter Password",
						 0);
			OnScreenKeyboard(pass,32); 
			
			// create account
			response=Ncard_createUser(user,pass);
			
			if(fp=fopen("sd:/apps/ncard/ncard1.dat","w"))
			{
				fwrite(user,strlen(user),1,fp);
				fclose(fp);
			}
			
			currentMenu=loginProcess(user, pass); 
						
		}
		
	}
	
	
	
	
	while(currentMenu != MENU_EXIT)
	{
		switch (currentMenu)
		{
			case MENU_SETTINGS:
				currentMenu = MenuSettings();
				break;
		
			default: // unrecognized menu
				currentMenu = MenuSettings();
				break;
		}
	}
	

	ResumeGui();
	ExitRequested = 1;
	while(1) usleep(THREAD_SLEEP);

	HaltGui();


	delete bgImg;
	delete mainWindow;

	delete pointer[0];
	delete pointer[1];
	delete pointer[2];
	delete pointer[3];

	mainWindow = NULL;
}
