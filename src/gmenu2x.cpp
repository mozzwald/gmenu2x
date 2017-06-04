/***************************************************************************
 *   Copyright (C) 2006 by Massimiliano Torromeo   *
 *   massimiliano.torromeo@gmail.com   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <SDL.h>
#include <SDL_gfxPrimitives.h>
#include <signal.h>

#include <sys/statvfs.h>
#include <errno.h>

#include <sys/fcntl.h> //for battery

//for browsing the filesystem
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

//for soundcard
#include <sys/ioctl.h>
#include <linux/soundcard.h>

#include "linkapp.h"
#include "linkaction.h"
#include "menu.h"
#include "fonthelper.h"
#include "surface.h"
#include "filedialog.h"
#include "gmenu2x.h"
#include "filelister.h"

#include "iconbutton.h"
#include "messagebox.h"
#include "inputdialog.h"
#include "settingsdialog.h"
#include "wallpaperdialog.h"
#include "textdialog.h"
#include "menusettingint.h"
#include "menusettingbool.h"
#include "menusettingrgba.h"
#include "menusettingstring.h"
#include "menusettingmultistring.h"
#include "menusettingfile.h"
#include "menusettingimage.h"
#include "menusettingdir.h"

#include "debug.h"

#include <sys/mman.h>

#define LINE_BUFSIZE 128

const char *CARD_ROOT = "/mnt/"; //Note: Add a trailing /!
const int CARD_ROOT_LEN = 5;

static GMenu2X *app;

using std::ifstream;
using std::ofstream;
using std::endl;
using std::setw;
using std::setfill;
using std::right;
using std::hex;
using std::ios_base;
using std::stringstream;
using namespace fastdelegate;

// Note: Keep this in sync with the enum!
static const char *colorNames[NUM_COLORS] = {
	"topBarBg",
	"bottomBarBg",
	"selectionBg",
	"messageBoxBg",
	"messageBoxBorder",
	"messageBoxSelection",
	"font",
	"fontOutline"
};

static enum color stringToColor(const string &name) {
	for (unsigned int i = 0; i < NUM_COLORS; i++) {
		if (strcmp(colorNames[i], name.c_str()) == 0) {
			return (enum color)i;
		}
	}
	return (enum color)-1;
}

static const char *colorToString(enum color c) {
	return colorNames[c];
}

static void quit_all(int err) {
	delete app;
	exit(err);
}

#ifdef TARGET_Z2
char *progpath = "/mnt/ffs/gmenu2x/";
const char sdcard[] = "/";
#endif

#if defined(TARGET_Z2) || defined(TARGET_IZ2S)
// Commander
#include "def.h"
#include "sdlutils.h"
#include "resourceManager.h"
#include "commander.h"

// Globals
SDL_Surface *Globals::g_screen = NULL;
const SDL_Color Globals::g_colorTextNormal = {COLOR_TEXT_NORMAL};
const SDL_Color Globals::g_colorTextTitle = {COLOR_TEXT_TITLE};
const SDL_Color Globals::g_colorTextDir = {COLOR_TEXT_DIR};
const SDL_Color Globals::g_colorTextSelected = {COLOR_TEXT_SELECTED};
std::vector<CWindow *> Globals::g_windows;

void GMenu2X::commander() {
    // Screen
    //Globals::g_screen = s->dblbuffer;

    // Create instances
    std::string l_path = getenv("HOME");
    CCommander l_commander(l_path, l_path);

    // Main loop
    l_commander.execute();
}
#endif

int main(int /*argc*/, char * /*argv*/[]) {
	INFO("GMenu2X starting: If you read this message in the logs, check http://mtorromeo.github.com/gmenu2x/troubleshooting.html for a solution");

	signal(SIGINT, &quit_all);
	signal(SIGSEGV,&quit_all);
	signal(SIGTERM,&quit_all);

#if defined(TARGET_IZ2S)
	progpath = get_current_dir_name();	
	strcat(progpath, "/");
#endif

	app = new GMenu2X();
	DEBUG("Starting main()");
	app->main();

	return 0;
}

void GMenu2X::gp2x_init() {
#if defined(TARGET_GP2X) || defined(TARGET_WIZ) || defined(TARGET_CAANOO)
	memdev = open("/dev/mem", O_RDWR);
	if (memdev < 0)
		WARNING("Could not open /dev/mem");
#endif

	if (memdev > 0) {
#ifdef TARGET_GP2X
		memregs = (unsigned short*)mmap(0, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
		MEM_REG=&memregs[0];
#elif defined(TARGET_WIZ) || defined(TARGET_CAANOO)
		memregs = (unsigned short*)mmap(0, 0x20000, PROT_READ|PROT_WRITE, MAP_SHARED, memdev, 0xc0000000);
#endif
		if (memregs == MAP_FAILED) {
			ERROR("Could not mmap hardware registers!");
			close(memdev);
		}
	}

#if defined(TARGET_GP2X)
	batteryHandle = open(f200 ? "/dev/mmsp2adc" : "/dev/batt", O_RDONLY);
	if (f200) {
		//if wm97xx fails to open, set f200 to false to prevent any further access to the touchscreen
		f200 = ts.init();
	}
#elif defined(TARGET_WIZ) || defined(TARGET_CAANOO)
	/* get access to battery device */
	batteryHandle = open("/dev/pollux_batt", O_RDONLY);
#endif
	INFO("System Init Done!");
}

void GMenu2X::gp2x_deinit() {
#ifdef TARGET_GP2X
	if (memdev > 0) {
		memregs[0x28DA>>1]=0x4AB;
		memregs[0x290C>>1]=640;
	}
	if (f200) ts.deinit();
#endif

	if (memdev > 0) {
		memregs = NULL;
		close(memdev);
	}
	if (batteryHandle!=0) close(batteryHandle);
}

void GMenu2X::gp2x_tvout_on(bool pal) {
#ifdef TARGET_GP2X
	if (memdev!=0) {
		/*Ioctl_Dummy_t *msg;
		#define FBMMSP2CTRL 0x4619
		int TVHandle = ioctl(SDL_videofd, FBMMSP2CTRL, msg);*/
		if (cx25874!=0) gp2x_tvout_off();
		//if tv-out is enabled without cx25874 open, stop
		//if (memregs[0x2800>>1]&0x100) return;
		cx25874 = open("/dev/cx25874",O_RDWR);
		ioctl(cx25874, _IOW('v', 0x02, unsigned char), pal ? 4 : 3);
		memregs[0x2906>>1]=512;
		memregs[0x28E4>>1]=memregs[0x290C>>1];
		memregs[0x28E8>>1]=239;
	}
#endif
}

void GMenu2X::gp2x_tvout_off() {
#ifdef TARGET_GP2X
	if (memdev!=0) {
		close(cx25874);
		cx25874 = 0;
		memregs[0x2906>>1]=1024;
	}
#endif
}

GMenu2X::GMenu2X() {
	//Detect firmware version and type
	if (fileExists("/etc/open2x")) {
		fwType = "open2x";
		fwVersion = "";
	} else {
		fwType = "gph";
		fwVersion = "";
	}
#ifdef TARGET_GP2X
	f200 = fileExists("/dev/touchscreen/wm97xx");
#else
	f200 = true;
#endif

	//open2x
	savedVolumeMode = 0;
	volumeMode = VOLUME_MODE_NORMAL;
	volumeScalerNormal = VOLUME_SCALER_NORMAL;
	volumeScalerPhones = VOLUME_SCALER_PHONES;

	o2x_usb_net_on_boot = false;
	o2x_usb_net_ip = "";
	o2x_ftp_on_boot = false;
	o2x_telnet_on_boot = false;
	o2x_gp2xjoy_on_boot = false;
	o2x_usb_host_on_boot = false;
	o2x_usb_hid_on_boot = false;
	o2x_usb_storage_on_boot = false;

	usbnet = samba = inet = web = false;
	useSelectionPng = false;

	//load config data
	readConfig();
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
	// Read the current mixer volume settings and ignore globalVolume.
	volumeScalerPhones=getVolume();
	volumeScalerNormal=getVolumeScaler();
	//volumeScalerNormal=2;
#else
	if (fwType=="open2x") {
		readConfigOpen2x();
		//	VOLUME MODIFIER
		switch(volumeMode) {
			case VOLUME_MODE_MUTE:   setVolumeScaler(VOLUME_SCALER_MUTE); break;
			case VOLUME_MODE_PHONES: setVolumeScaler(volumeScalerPhones);	break;
			case VOLUME_MODE_NORMAL: setVolumeScaler(volumeScalerNormal); break;
		}
	}
#if defined(TARGET_GP2x)
	else
		readCommonIni();
#endif
#endif

	halfX = resX/2;
	halfY = resY/2;
	bottomBarIconY = resY-18;
	bottomBarTextY = resY-10;

	path = "";
	getExePath();

#if defined(TARGET_GP2X)
	cx25874 = 0;
#endif
	batteryHandle = 0;
	memdev = 0;
#if defined(TARGET_GP2X) || defined(TARGET_WIZ) || defined(TARGET_CAANOO)
	gp2x_init();
#endif

#ifdef TARGET_GP2X
	//Fix tv-out
	if (memdev > 0) {
		if (memregs[0x2800>>1]&0x100) {
			memregs[0x2906>>1]=512;
			//memregs[0x290C>>1]=640;
			memregs[0x28E4>>1]=memregs[0x290C>>1];
		}
		memregs[0x28E8>>1]=239;
	}
#endif

#if !defined(TARGET_PC)
	setenv("SDL_NOMOUSE", "1", 1);
#endif
	//Screen
	if( SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_JOYSTICK)<0 ) {
		ERROR("Could not initialize SDL: %s", SDL_GetError());
		quit();
	}

	s = new Surface();
#if defined(TARGET_GP2X) || defined(TARGET_WIZ) || defined(TARGET_CAANOO) || defined(TARGET_Z2)
	{
		//I'm forced to use SW surfaces since with HW there are issuse with changing the clock frequency
		SDL_Surface *dbl = SDL_SetVideoMode(resX, resY, confInt["videoBpp"], SDL_SWSURFACE);
		s->enableVirtualDoubleBuffer(dbl);
		SDL_ShowCursor(0);
		
		Globals::g_screen = dbl;

	}
#else
	s->raw = SDL_SetVideoMode(resX, resY, confInt["videoBpp"], SDL_HWSURFACE|SDL_DOUBLEBUF);
#endif

	bg = NULL;
	btnContextMenu = NULL;
	font = NULL;
	menu = NULL;
	setSkin(confStr["skin"], false);
	initMenu();

	if (!fileExists(confStr["wallpaper"])) {
		DEBUG("Searching wallpaper");

		FileLister fl("skins/"+confStr["skin"]+"/wallpapers",false,true);
		fl.setFilter(".png,.jpg,.jpeg,.bmp");
		fl.browse();
		if (fl.getFiles().size()<=0 && confStr["skin"] != "Default")
			fl.setPath("skins/Default/wallpapers",true);
		if (fl.getFiles().size()>0)
			confStr["wallpaper"] = fl.getPath()+fl.getFiles()[0];
	}

	initBG();
	input.init(path+"input.conf");
	setInputSpeed();
	initServices();

	setGamma(confInt["gamma"]);
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
	// Read the current mixer volume settings and ignore globalVolume.
#else
	setVolume(confInt["globalVolume"]);
#endif
	applyDefaultTimings();
	setClock(confInt["menuClock"]);

	//recover last session
	readTmp();
	if (lastSelectorElement>-1 && menu->selLinkApp()!=NULL && (!menu->selLinkApp()->getSelectorDir().empty() || !lastSelectorDir.empty()))
		menu->selLinkApp()->selector(lastSelectorElement,lastSelectorDir);

	bRedraw=true;
}

GMenu2X::~GMenu2X() {
	writeConfig();
	if (fwType=="open2x") writeConfigOpen2x();

	quit();

	delete menu;
	delete s;
	delete font;
}

void GMenu2X::quit() {
	fflush(NULL);
	sc.clear();
	s->free();
	SDL_Quit();
#ifdef TARGET_GP2X
	if (memdev!=0) {
		//Fix tv-out
		if (memregs[0x2800>>1]&0x100) {
			memregs[0x2906>>1]=512;
			memregs[0x28E4>>1]=memregs[0x290C>>1];
		}
		gp2x_deinit();
	}
#endif
}

void GMenu2X::initBG() {
	sc.del("bgmain");

	if (bg != NULL) delete bg;

	bg = new Surface(s);
	bg->box(0,0,resX,resY,0,0,0);
	if (fileExists(confStr["wallpaper"])) {
		Surface wall(confStr["wallpaper"],false);
		wall.blit(bg,0,0);
	}

	drawTopBar(bg);
	drawBottomBar(bg);

	Surface *bgmain = new Surface(bg);
	sc.add(bgmain,"bgmain");

	Surface sd("imgs/sd.png", confStr["skin"]);
#ifdef TARGET_IZ2S /* ZIPIT_Z2_VOLUME */
	Surface cpu("imgs/volume.png", confStr["skin"]);
	Surface volume("imgs/phones.png", confStr["skin"]);
#else
	Surface cpu("imgs/cpu.png", confStr["skin"]);
	Surface volume("imgs/volume.png", confStr["skin"]);
#endif
	string df = getDiskFree();

	sd.blit( sc["bgmain"], 3, bottomBarIconY );
	sc["bgmain"]->write( font, df, 22, bottomBarTextY, HAlignLeft, VAlignMiddle );
	volumeX = 27+font->getTextWidth(df);
	volume.blit( sc["bgmain"], volumeX, bottomBarIconY );
	volumeX += 19;
	cpuX = volumeX+font->getTextWidth("100")+5;
	cpu.blit( sc["bgmain"], cpuX, bottomBarIconY );
	cpuX += 19;
#ifdef TARGET_IZ2S /* ZIPIT_Z2_VOLUME */
	manualX = cpuX+font->getTextWidth("300Mhz")+5;
#else
	manualX = cpuX+font->getTextWidth("100")+5;
#endif

#if defined(TARGET_GP2X)
	int serviceX = resX-38;
	if (usbnet) {
		if (web) {
			Surface webserver("imgs/webserver.png", confStr["skin"]);
			webserver.blit( sc["bgmain"], serviceX, bottomBarIconY );
			serviceX -= 19;
		}
		if (samba) {
			Surface sambaS("imgs/samba.png", confStr["skin"]);
			sambaS.blit( sc["bgmain"], serviceX, bottomBarIconY );
			serviceX -= 19;
		}
		if (inet) {
			Surface inetS("imgs/inet.png", confStr["skin"]);
			inetS.blit( sc["bgmain"], serviceX, bottomBarIconY );
			serviceX -= 19;
		}
	}
#endif
}

void GMenu2X::initFont() {
	if (font != NULL) {
		delete font;
		font = NULL;
	}
	font = new FontHelper(sc.getSkinFilePath("font.ttf"), skinConfInt["fontSize"], skinConfColors[COLOR_FONT], skinConfColors[COLOR_FONT_OUTLINE]);
}

void GMenu2X::initMenu() {
	//Menu structure handler
	menu = new Menu(this);
	for (uint i=0; i<menu->getSections().size(); i++) {
		//Add virtual links in the applications section
#if 1 /* ZIPIT_Z2 (IZ2S) */
		if ((menu->getSections()[i]=="applications") || (menu->getSections()[i]=="apps")) {
#else
		if (menu->getSections()[i]=="applications") {
#endif
			menu->addActionLink(i,"Explorer",MakeDelegate(this,&GMenu2X::explorer),tr["Launch an application"],"skin:icons/explorer.png");
#if 1 /* ZIPIT_Z2 (IZ2S) */
			menu->addActionLink(i,"Files",MakeDelegate(this,&GMenu2X::commander),tr["File Manager"],"skin:icons/section.png");
#endif
		}

		//Add virtual links in the setting section
		else if (menu->getSections()[i]=="settings") {
			menu->addActionLink(i,"GMenu2X",MakeDelegate(this,&GMenu2X::options),tr["Configure GMenu2X's options"],"skin:icons/configure.png");
			if (fwType=="open2x")
				menu->addActionLink(i,"Open2x",MakeDelegate(this,&GMenu2X::settingsOpen2x),tr["Configure Open2x system settings"],"skin:icons/o2xconfigure.png");
			menu->addActionLink(i,tr["Skin"],MakeDelegate(this,&GMenu2X::skinMenu),tr["Configure skin"],"skin:icons/skin.png");
			menu->addActionLink(i,tr["Wallpaper"],MakeDelegate(this,&GMenu2X::changeWallpaper),tr["Change GMenu2X wallpaper"],"skin:icons/wallpaper.png");
#ifdef TARGET_GP2X
			menu->addActionLink(i,"TV",MakeDelegate(this,&GMenu2X::toggleTvOut),tr["Activate/deactivate tv-out"],"skin:icons/tv.png");
			menu->addActionLink(i,"USB Sd",MakeDelegate(this,&GMenu2X::activateSdUsb),tr["Activate Usb on SD"],"skin:icons/usb.png");
			if (fwType=="gph" && !f200)
				menu->addActionLink(i,"USB Nand",MakeDelegate(this,&GMenu2X::activateNandUsb),tr["Activate Usb on Nand"],"skin:icons/usb.png");
			//menu->addActionLink(i,"USB Root",MakeDelegate(this,&GMenu2X::activateRootUsb),tr["Activate Usb on the root of the Gp2x Filesystem"],"skin:icons/usb.png");
#endif
			if (fileExists(path+"log.txt"))
				menu->addActionLink(i,tr["Log Viewer"],MakeDelegate(this,&GMenu2X::viewLog),tr["Displays last launched program's output"],"skin:icons/ebook.png");
			menu->addActionLink(i,tr["About"],MakeDelegate(this,&GMenu2X::about),tr["Info about GMenu2X"],"skin:icons/about.png");
		}
	}

	menu->setSectionIndex(confInt["section"]);
	menu->setLinkIndex(confInt["link"]);

	menu->loadIcons();

#if 1 /* ZIPIT_Z2 (IZ2S) */
	// Commander resources
	CResourceManager::instance();
#endif

}

void GMenu2X::about() {
	vector<string> text;
	string temp;
	temp = "GMenu2X is developed by Massimiliano \"Ryo\" Torromeo, and is released under the GPL-v2 license.\n\
Website: http://mtorromeo.github.com/gmenu2x\n\
E-Mail & PayPal account: massimiliano.torromeo@gmail.com\n";
#if defined(TARGET_CAANOO)
	string versionFile = "";
	if (fileExists("/usr/gp2x/version"))
		versionFile = "/usr/gp2x/version";
	else if (fileExists("/tmp/gp2x/version"))
		versionFile = "/tmp/gp2x/version";
	if (!versionFile.empty()) {
		ifstream f(versionFile.c_str(), ios_base::in);
		if (f.is_open()) {
			string line;
			if (getline(f, line, '\n'))
				temp += "\nFirmware version: " + line + "\n" + exec("uname -srm");
			f.close();
		}
	}
#endif
	temp += "\n\
Thanks goes to...\n\
\n\
 Contributors\n\
----\n\
NoidZ for his gp2x' buttons graphics\n\
Pickle for the initial Wiz and Caanoo ports\n\
\n\
 Beta testers\n\
----\n\
Goemon4, PokeParadox, PSyMastR and Tripmonkey_uk\n\
\n\
 Translators\n\
----\n\
English & Italian by me\n\
French by Yodaz\n\
Danish by claus\n\
Dutch by superfly\n\
Spanish by pedator\n\
Portuguese (Portugal) by NightShadow\n\
Slovak by Jozef\n\
Swedish by Esslan and Micket\n\
German by fusion_power, johnnysnet and Waldteufel\n\
Finnish by Jontte and Atte\n\
Norwegian by cowai\n\
Russian by XaMMaX90\n\
\n\
 Donors\n\
----\n\
EvilDragon (www.gp2x.de)\n\
Tecnologie Creative (www.tecnologiecreative.it)\n\
TelcoLou\n\
gaterooze\n\
deepmenace\n\
superfly\n\
halo9\n\
sbock\n\
b._.o._.b\n\
Jacopastorius\n\
lorystorm90\n\
and all the anonymous donors...\n\
(If I missed to list you or if you want to be removed, contact me.)";
	split(text, temp, "\n");
	TextDialog td(this, "GMenu2X", tr.translate("Version $1 (Build date: $2)","0.11",__DATE__,NULL), "icons/about.png", &text);
	td.exec();
}

void GMenu2X::viewLog() {
	string logfile = path+"log.txt";
	if (!fileExists(logfile)) return;
	ifstream inf(logfile.c_str(), ios_base::in);
	if (inf.is_open()) return;
	vector<string> log;

	string line;
	while (getline(inf, line, '\n'))
		log.push_back(line);
	inf.close();

	TextDialog td(this, tr["Log Viewer"], tr["Displays last launched program's output"], "icons/ebook.png", &log);
	td.exec();

	MessageBox mb(this, tr["Do you want to delete the log file?"], "icons/ebook.png");
	mb.setButton(CONFIRM, tr["Yes"]);
	mb.setButton(CANCEL,  tr["No"]);
	if (mb.exec() == CONFIRM) {
		ledOn();
		unlink(logfile.c_str());
		sync();
		menu->deleteSelectedLink();
		ledOff();
	}
}

void GMenu2X::readConfig() {
	string conffile = path+"gmenu2x.conf";
	if (fileExists(conffile)) {
		ifstream inf(conffile.c_str(), ios_base::in);
		if (inf.is_open()) {
			string line;
			while (getline(inf, line, '\n')) {
				string::size_type pos = line.find("=");
				string name = trim(line.substr(0,pos));
				string value = trim(line.substr(pos+1,line.length()));

				if (value.length()>1 && value.at(0)=='"' && value.at(value.length()-1)=='"')
					confStr[name] = value.substr(1,value.length()-2);
				else
					confInt[name] = atoi(value.c_str());
			}
			inf.close();
		}
	}

	if (!confStr["lang"].empty()) tr.setLang(confStr["lang"]);
	if (!confStr["wallpaper"].empty() && !fileExists(confStr["wallpaper"])) confStr["wallpaper"] = "";
	if (confStr["skin"].empty() || !fileExists("skins/"+confStr["skin"])) confStr["skin"] = "Default";

	evalIntConf( &confInt["outputLogs"], 0, 0,1 );
#ifdef TARGET_GP2X
	evalIntConf( &confInt["maxClock"], 300, 200,300 );
	evalIntConf( &confInt["menuClock"], f200 ? 136 : 100, 50,300 );
#elif defined(TARGET_WIZ) || defined(TARGET_CAANOO)
	evalIntConf( &confInt["maxClock"], 900, 200,900 );
	evalIntConf( &confInt["menuClock"], DEFAULT_CPU_CLK, 50,300 );
#elif defined(TARGET_Z2)
	evalIntConf( &confInt["maxClock"], 416, 200,416 );
	evalIntConf( &confInt["menuClock"], DEFAULT_CPU_CLK, 50,312 );
#endif
	evalIntConf( &confInt["globalVolume"], 67, 0,100 );
	evalIntConf( &confInt["gamma"], 10, 1,100 );
	evalIntConf( &confInt["videoBpp"], 16, 8,32 );

	if (confStr["tvoutEncoding"] != "PAL") confStr["tvoutEncoding"] = "NTSC";
	resX = constrain( confInt["resolutionX"], 320,1920 );
	resY = constrain( confInt["resolutionY"], 240,1200 );
}

void GMenu2X::writeConfig() {
	ledOn();
	string conffile = path+"gmenu2x.conf";
	ofstream inf(conffile.c_str());
	if (inf.is_open()) {
		ConfStrHash::iterator endS = confStr.end();
		for(ConfStrHash::iterator curr = confStr.begin(); curr != endS; curr++)
			inf << curr->first << "=\"" << curr->second << "\"" << endl;

		ConfIntHash::iterator endI = confInt.end();
		for(ConfIntHash::iterator curr = confInt.begin(); curr != endI; curr++)
			inf << curr->first << "=" << curr->second << endl;

		inf.close();
		sync();
	}
	ledOff();
}


void GMenu2X::readConfigOpen2x() {
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
#else
	string conffile = "/etc/config/open2x.conf";
	if (!fileExists(conffile)) return;
	ifstream inf(conffile.c_str(), ios_base::in);
	if (!inf.is_open()) return;
	string line;
	while (getline(inf, line, '\n')) {
		string::size_type pos = line.find("=");
		string name = trim(line.substr(0,pos));
		string value = trim(line.substr(pos+1,line.length()));

		if (name=="USB_NET_ON_BOOT") o2x_usb_net_on_boot = value == "y" ? true : false;
		else if (name=="USB_NET_IP") o2x_usb_net_ip = value;
		else if (name=="TELNET_ON_BOOT") o2x_telnet_on_boot = value == "y" ? true : false;
		else if (name=="FTP_ON_BOOT") o2x_ftp_on_boot = value == "y" ? true : false;
		else if (name=="GP2XJOY_ON_BOOT") o2x_gp2xjoy_on_boot = value == "y" ? true : false;
		else if (name=="USB_HOST_ON_BOOT") o2x_usb_host_on_boot = value == "y" ? true : false;
		else if (name=="USB_HID_ON_BOOT") o2x_usb_hid_on_boot = value == "y" ? true : false;
		else if (name=="USB_STORAGE_ON_BOOT") o2x_usb_storage_on_boot = value == "y" ? true : false;
		else if (name=="VOLUME_MODE") volumeMode = savedVolumeMode = constrain( atoi(value.c_str()), 0, 2);
		else if (name=="PHONES_VALUE") volumeScalerPhones = constrain( atoi(value.c_str()), 0, 100);
		else if (name=="NORMAL_VALUE") volumeScalerNormal = constrain( atoi(value.c_str()), 0, 150);
	}
	inf.close();
#endif
}
void GMenu2X::writeConfigOpen2x() {
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
#else
	ledOn();
	string conffile = "/etc/config/open2x.conf";
	ofstream inf(conffile.c_str());
	if (inf.is_open()) {
		inf << "USB_NET_ON_BOOT=" << ( o2x_usb_net_on_boot ? "y" : "n" ) << endl;
		inf << "USB_NET_IP=" << o2x_usb_net_ip << endl;
		inf << "TELNET_ON_BOOT=" << ( o2x_telnet_on_boot ? "y" : "n" ) << endl;
		inf << "FTP_ON_BOOT=" << ( o2x_ftp_on_boot ? "y" : "n" ) << endl;
		inf << "GP2XJOY_ON_BOOT=" << ( o2x_gp2xjoy_on_boot ? "y" : "n" ) << endl;
		inf << "USB_HOST_ON_BOOT=" << ( (o2x_usb_host_on_boot || o2x_usb_hid_on_boot || o2x_usb_storage_on_boot) ? "y" : "n" ) << endl;
		inf << "USB_HID_ON_BOOT=" << ( o2x_usb_hid_on_boot ? "y" : "n" ) << endl;
		inf << "USB_STORAGE_ON_BOOT=" << ( o2x_usb_storage_on_boot ? "y" : "n" ) << endl;
		inf << "VOLUME_MODE=" << volumeMode << endl;
		if (volumeScalerPhones != VOLUME_SCALER_PHONES) inf << "PHONES_VALUE=" << volumeScalerPhones << endl;
		if (volumeScalerNormal != VOLUME_SCALER_NORMAL) inf << "NORMAL_VALUE=" << volumeScalerNormal << endl;
		inf.close();
		sync();
	}
	ledOff();
#endif
}

void GMenu2X::writeSkinConfig() {
	ledOn();
	string conffile = path+"skins/"+confStr["skin"]+"/skin.conf";
	ofstream inf(conffile.c_str());
	if (inf.is_open()) {
		ConfStrHash::iterator endS = skinConfStr.end();
		for(ConfStrHash::iterator curr = skinConfStr.begin(); curr != endS; curr++)
			inf << curr->first << "=\"" << curr->second << "\"" << endl;

		ConfIntHash::iterator endI = skinConfInt.end();
		for(ConfIntHash::iterator curr = skinConfInt.begin(); curr != endI; curr++)
			inf << curr->first << "=" << curr->second << endl;

		for (int i = 0; i < NUM_COLORS; ++i) {
			inf << colorToString((enum color)i) << "=#"
				<< hex << setw(2) << setfill('0') << right << skinConfColors[i].r
				<< hex << setw(2) << setfill('0') << right << skinConfColors[i].g
				<< hex << setw(2) << setfill('0') << right << skinConfColors[i].b
				<< hex << setw(2) << setfill('0') << right << skinConfColors[i].a << endl;
		}


		inf.close();
		sync();
	}
	ledOff();
}

void GMenu2X::readCommonIni() {
	if (!fileExists("/usr/gp2x/common.ini")) return;
	ifstream inf("/usr/gp2x/common.ini", ios_base::in);
	if (!inf.is_open()) return;
	string line;
	string section = "";
	while (getline(inf, line, '\n')) {
		line = trim(line);
		if (line[0]=='[' && line[line.length()-1]==']') {
			section = line.substr(1,line.length()-2);
		} else {
			string::size_type pos = line.find("=");
			string name = trim(line.substr(0,pos));
			string value = trim(line.substr(pos+1,line.length()));

			if (section=="usbnet") {
				if (name=="enable")
					usbnet = value=="true" ? true : false;
				else if (name=="ip")
					ip = value;

			} else if (section=="server") {
				if (name=="inet")
					inet = value=="true" ? true : false;
				else if (name=="samba")
					samba = value=="true" ? true : false;
				else if (name=="web")
					web = value=="true" ? true : false;
			}
		}
	}
	inf.close();
}

void GMenu2X::writeCommonIni() {}

void GMenu2X::readTmp() {
	lastSelectorElement = -1;
	if (!fileExists("/tmp/gmenu2x.tmp")) return;
	ifstream inf("/tmp/gmenu2x.tmp", ios_base::in);
	if (!inf.is_open()) return;
	string line;
	string section = "";
	while (getline(inf, line, '\n')) {
		string::size_type pos = line.find("=");
		string name = trim(line.substr(0,pos));
		string value = trim(line.substr(pos+1,line.length()));

		if (name=="section")
			menu->setSectionIndex(atoi(value.c_str()));
		else if (name=="link")
			menu->setLinkIndex(atoi(value.c_str()));
		else if (name=="selectorelem")
			lastSelectorElement = atoi(value.c_str());
		else if (name=="selectordir")
			lastSelectorDir = value;
	}
	inf.close();
	unlink("/tmp/gmenu2x.tmp");
}

void GMenu2X::writeTmp(int selelem, const string &selectordir) {
	string conffile = "/tmp/gmenu2x.tmp";
	ofstream inf(conffile.c_str());
	if (inf.is_open()) {
		inf << "section=" << menu->selSectionIndex() << endl;
		inf << "link=" << menu->selLinkIndex() << endl;
		if (selelem>-1)
			inf << "selectorelem=" << selelem << endl;
		if (selectordir!="")
			inf << "selectordir=" << selectordir << endl;
		inf.close();
	}
}

void GMenu2X::initServices() {
#ifdef TARGET_GP2X
	if (usbnet) {
		string services = "scripts/services.sh "+ip+" "+(inet?"on":"off")+" "+(samba?"on":"off")+" "+(web?"on":"off")+" &";
		system(services.c_str());
	}
#endif
}

void GMenu2X::ledOn() {
#ifdef TARGET_GP2X
	if (memdev!=0 && !f200) memregs[0x106E >> 1] ^= 16;
	//SDL_SYS_JoystickGp2xSys(joy.joystick, BATT_LED_ON);
#endif
}

void GMenu2X::ledOff() {
#ifdef TARGET_GP2X
	if (memdev!=0 && !f200) memregs[0x106E >> 1] ^= 16;
	//SDL_SYS_JoystickGp2xSys(joy.joystick, BATT_LED_OFF);
#endif
}

void GMenu2X::getTime(char* strTime, int len){
	char am_pm[10]="AM";
	time_t t = time(NULL);
	struct tm theLocalTime = *localtime(&t);
	int nHr = theLocalTime.tm_hour;

	if(theLocalTime.tm_hour >= 12){
		am_pm[0]='P';
		nHr %=12;
	}

	if(nHr == 0)
		nHr = 12;

	snprintf(strTime, len, "%2d:%02d %s", nHr, theLocalTime.tm_min, am_pm);
}

void GMenu2X::main() {
	uint linksPerPage = linkColumns*linkRows;
	int linkSpacingX = (resX-10 - linkColumns*skinConfInt["linkWidth"])/linkColumns;
	int linkSpacingY = (resY-35 - skinConfInt["topBarHeight"] - linkRows*skinConfInt["linkHeight"])/linkRows;
	uint sectionLinkPadding = max(skinConfInt["topBarHeight"] - 32 - font->getHeight(), 0) / 3;

	bool quit = false;
	int x,y, offset = menu->sectionLinks()->size()>linksPerPage ? 2 : 6, helpBoxHeight = fwType=="open2x" ? 154 : 139;
	uint i;
	uint nloops = 0;
	long tickBattery = -60000, tickNow;
	string batteryIcon = "imgs/battery/0.png";
#if defined(TARGET_Z2) || defined(TARGET_IZ2S)
	string wifiIcon = "imgs/wifi/off.png";
#endif
#ifdef TARGET_Z2
	string cpuIcon = "imgs/cpu/0.png";
	char strTime[20];
	getTime(strTime, sizeof(strTime));
#endif
	stringstream ss;
	uint sectionsCoordX = 24;
	SDL_Rect re = {0,0,0,0};

	btnContextMenu = new IconButton(this,"skin:imgs/menu.png");
	btnContextMenu->setPosition(resX-38, bottomBarIconY);
	btnContextMenu->setAction(MakeDelegate(this, &GMenu2X::contextMenu));

	nbattlevel = getBatteryLevel();
	nwifilevel = getWiFiLevel();
	nMHz = getCPUspeed();
	bRedraw = true;

	while (!quit) {
		tickNow = SDL_GetTicks();

		/* Only redraw the screen when we really need to and
		 * if gmenu2x is the currently active tty */
		if(bRedraw && (SDL_GetAppState() & SDL_APPACTIVE)){
			//Background
			sc["bgmain"]->blit(s,0,0);

			//Sections
			sectionsCoordX = halfX - (constrain((uint)menu->getSections().size(), 0 , linkColumns) * skinConfInt["linkWidth"]) / 2;
			if (menu->firstDispSection()>0)
				sc.skinRes("imgs/l_enabled.png")->blit(s,0,0);
			else
				sc.skinRes("imgs/l_disabled.png")->blit(s,0,0);
			if (menu->firstDispSection()+linkColumns<menu->getSections().size())
				sc.skinRes("imgs/r_enabled.png")->blit(s,resX-10,0);
			else
				sc.skinRes("imgs/r_disabled.png")->blit(s,resX-10,0);

			for (i=menu->firstDispSection(); i<menu->getSections().size() && i<menu->firstDispSection()+linkColumns; i++) {
				string sectionIcon = "skin:sections/"+menu->getSections()[i]+".png";
				x = (i-menu->firstDispSection())*skinConfInt["linkWidth"]+sectionsCoordX;
				if (menu->selSectionIndex()==(int)i)
					s->box(x, 0, skinConfInt["linkWidth"],
					skinConfInt["topBarHeight"], skinConfColors[COLOR_SELECTION_BG]);
				x += skinConfInt["linkWidth"]/2;
				if (sc.exists(sectionIcon))
					sc[sectionIcon]->blit(s,x-16,sectionLinkPadding,32,32);
				else
					sc.skinRes("icons/section.png")->blit(s,x-16,sectionLinkPadding,32,32);
				s->write( font, menu->getSections()[i], x, skinConfInt["topBarHeight"]-sectionLinkPadding, HAlignCenter, VAlignBottom );
			}

			//Links
			s->setClipRect(offset,skinConfInt["topBarHeight"],resX-9,resY-74); //32*2+10
			for (i=menu->firstDispRow()*linkColumns; i<(menu->firstDispRow()*linkColumns)+linksPerPage && i<menu->sectionLinks()->size(); i++) {
				int ir = i-menu->firstDispRow()*linkColumns;
				x = (ir%linkColumns)*(skinConfInt["linkWidth"]+linkSpacingX)+offset;
				y = ir/linkColumns*(skinConfInt["linkHeight"]+linkSpacingY)+skinConfInt["topBarHeight"]+2;
				menu->sectionLinks()->at(i)->setPosition(x,y);

				if (i==(uint)menu->selLinkIndex())
					menu->sectionLinks()->at(i)->paintHover();

				menu->sectionLinks()->at(i)->paint();
			}
			s->clearClipRect();

			drawScrollBar(linkRows,menu->sectionLinks()->size()/linkColumns + ((menu->sectionLinks()->size()%linkColumns==0) ? 0 : 1),menu->firstDispRow(),43,resY-81);

#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
			// Handy spot for the mute icon.
			//sc.skinRes("imgs/mute.png")->blit(s,resX-56,bottomBarIconY);
#else
			if (fwType=="open2x") {
				switch(volumeMode) {
					case VOLUME_MODE_MUTE:   sc.skinRes("imgs/mute.png")->blit(s,resX-56,bottomBarIconY); break;
					case VOLUME_MODE_PHONES: sc.skinRes("imgs/phones.png")->blit(s,resX-56,bottomBarIconY); break;
					default: sc.skinRes("imgs/volume.png")->blit(s,resX-56,bottomBarIconY); break;
				}
			}
#endif
#ifdef TARGET_IZ2S /* ZIPIT_Z2_VOLUME */
			if (menu->selLink()!=NULL) {
				s->write ( font, menu->selLink()->getDescription(), halfX, resY-19, HAlignCenter, VAlignBottom );
				{ // zipit volume is independent of menu->selLinkApp()
					stringstream ss;
					string speakerStr = "";
					string phonesStr = "";
					ss.clear();
					ss << volumeScalerNormal;
					ss >> speakerStr;
					s->write ( font, speakerStr, cpuX, bottomBarTextY, HAlignLeft, VAlignMiddle );
					ss.clear();
					ss << volumeScalerPhones;
					ss >> phonesStr;
					s->write ( font, phonesStr, volumeX, bottomBarTextY, HAlignLeft, VAlignMiddle );
				}
				if (menu->selLinkApp()!=NULL) {
					//Manual indicator
					if (!menu->selLinkApp()->getManual().empty())
						sc.skinRes("imgs/manual.png")->blit(s,manualX,bottomBarIconY);
				}
			}
#else
			if (menu->selLink()!=NULL) {
				s->write ( font, menu->selLink()->getDescription(), halfX, resY-19, HAlignCenter, VAlignBottom );
				if (menu->selLinkApp()!=NULL) {
#ifndef TARGET_Z2
					s->write ( font, menu->selLinkApp()->clockStr(confInt["maxClock"]), cpuX, bottomBarTextY, HAlignLeft, VAlignMiddle );
					s->write ( font, menu->selLinkApp()->volumeStr(), volumeX, bottomBarTextY, HAlignLeft, VAlignMiddle );
#endif
					//Manual indicator
					if (!menu->selLinkApp()->getManual().empty())
						sc.skinRes("imgs/manual.png")->blit(s,manualX,bottomBarIconY);
				}
			}
#endif

#ifdef TOUCHSCREEN
			if (f200) {
				btnContextMenu->paint();
			}
#endif

#ifdef TARGET_Z2
			// draw cpu speed icon
			char cpuspeed[3];
			snprintf(cpuspeed, sizeof(cpuspeed), "%d", nMHz);
			cpuIcon = "imgs/cpu/"+string(cpuspeed)+".png";
			sc.skinRes(cpuIcon)->blit( s, cpuX-19, bottomBarIconY );

			//draw the time
			s->write ( font, strTime, manualX+19*2, bottomBarTextY, HAlignLeft, VAlignMiddle );
			sc.skinRes("imgs/clock.png")->blit( s, manualX+19, bottomBarIconY );
#endif
#if defined(TARGET_Z2) || defined(TARGET_IZ2S)
			// draw wifi status/signal level
			if (nwifilevel == 0)
				wifiIcon = "imgs/wifi/off.png";
			else {
				char wifilevel[3];
				snprintf(wifilevel, sizeof(wifilevel), "%d", nwifilevel);
				wifiIcon = "imgs/wifi/"+string(wifilevel)+".png";
			}
			sc.skinRes(wifiIcon)->blit( s, resX-19*3, bottomBarIconY );
#endif
			// draw battery status
			if (nbattlevel == 6) {
				batteryIcon = "imgs/battery/ac_chrg.png";
			} else if (nbattlevel == 7){
				batteryIcon = "imgs/battery/ac_full.png";
			} else {
				ss.clear();
				ss << nbattlevel;
				ss >> batteryIcon;
				batteryIcon = "imgs/battery/"+batteryIcon+".png";
			}
			sc.skinRes(batteryIcon)->blit( s, resX-19, bottomBarIconY );

			//On Screen Help
			if (input.isActive(MODIFIER)) {
				s->box(10,50,300,143, skinConfColors[COLOR_MESSAGE_BOX_BG]);
				s->rectangle( 12,52,296,helpBoxHeight, skinConfColors[COLOR_MESSAGE_BOX_BORDER] );
				s->write( font, tr["CONTROLS"], 20, 60 );
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
				s->write( font, tr["Enter  -   Launch link / Confirm action"], 20, 80 );
				s->write( font, tr["<<  >>  -   Change section"], 20, 95 );
				s->write( font, tr["H     -   Show manual/readme"], 20, 110 );
				s->write( font, tr["Tab   -   Show contextual menu"], 20, 125 );
				s->write( font, tr["Space  -   Show options menu"], 20, 140 );
				s->write( font, tr["VolUp, VolDown  -  Change headphone volume"], 20, 155 );
				s->write( font, tr["Ctrl-VolUp, VolDown  -  Speaker volume"], 20, 170 );
#else
				s->write( font, tr["B, Stick press: Launch link / Confirm action"], 20, 80 );
				s->write( font, tr["L, R: Change section"], 20, 95 );
				s->write( font, tr["Y: Show manual/readme"], 20, 110 );
				s->write( font, tr["VOLUP, VOLDOWN: Change cpu clock"], 20, 125 );
				s->write( font, tr["A+VOLUP, A+VOLDOWN: Change volume"], 20, 140 );
				s->write( font, tr["SELECT: Show contextual menu"], 20, 155 );
				s->write( font, tr["START: Show options menu"], 20, 170 );
				if (fwType=="open2x") s->write( font, tr["X: Toggle speaker mode"], 20, 185 );
#endif
			}

			s->flip();
			bRedraw = false;
		} //end bRedraw

#ifdef TOUCHSCREEN
		//touchscreen
		if (f200) {
			ts.poll();
			btnContextMenu->handleTS();
			re.x = 0; re.y = 0; re.h = skinConfInt["topBarHeight"]; re.w = resX;
			if (ts.pressed() && ts.inRect(re)) {
				re.w = skinConfInt["linkWidth"];
				for (i=menu->firstDispSection(); !ts.handled() && i<menu->getSections().size() && i<menu->firstDispSection()+linkColumns; i++) {
					sectionsCoordX = halfX - (constrain((uint)menu->getSections().size(), 0 , linkColumns) * skinConfInt["linkWidth"]) / 2;
					re.x = (i-menu->firstDispSection())*re.w+sectionsCoordX;

					if (ts.inRect(re)) {
						menu->setSectionIndex(i);
						ts.setHandled();
					}
				}
			}

			i=menu->firstDispRow()*linkColumns;
			while ( i<(menu->firstDispRow()*linkColumns)+linksPerPage && i<menu->sectionLinks()->size()) {
				if (menu->sectionLinks()->at(i)->isPressed())
					menu->setLinkIndex(i);
				if (menu->sectionLinks()->at(i)->handleTS())
					i = menu->sectionLinks()->size();
				i++;
			}
		}
#endif
		if ( input.update(false) ) bRedraw = true;
		if ( input[CONFIRM] && menu->selLink()!=NULL ) menu->selLink()->run();
		else if ( input[WIFI_SETUP] )	wifiSetup();
		else if ( input[SETTINGS]  ) options();
		else if ( input[MENU] ) contextMenu();
		// VOLUME SCALE MODIFIER
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
		// Read the current mixer volume settings.  Maybe do MUTE here?
		else if ( fwType=="open2x" && input[CANCEL] ) {
			volumeScalerPhones=getVolume();
			volumeScalerNormal=getVolumeScaler();
		}
#else
		else if ( fwType=="open2x" && input[CANCEL] ) {
			volumeMode = constrain(volumeMode-1, -VOLUME_MODE_MUTE-1, VOLUME_MODE_NORMAL);
			if(volumeMode < VOLUME_MODE_MUTE)
				volumeMode = VOLUME_MODE_NORMAL;
			switch(volumeMode) {
				case VOLUME_MODE_MUTE:   setVolumeScaler(VOLUME_SCALER_MUTE); break;
				case VOLUME_MODE_PHONES: setVolumeScaler(volumeScalerPhones); break;
				case VOLUME_MODE_NORMAL: setVolumeScaler(volumeScalerNormal); break;
			}
			setVolume(confInt["globalVolume"]);
		}
#endif
		// LINK NAVIGATION
		else if ( input[LEFT ]  ) menu->linkLeft();
		else if ( input[RIGHT]  ) menu->linkRight();
		else if ( input[UP   ]  ) menu->linkUp();
		else if ( input[DOWN ]  ) menu->linkDown();
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
		// Speaker VOLUME
		else if ( input[VOLDOWN] && input.isActive(MODIFIER) )
			setVolumeScaler( constrain( volumeScalerNormal-1, 0, 100));
		else if ( input[VOLUP] && input.isActive(MODIFIER) )
			setVolumeScaler( constrain( volumeScalerNormal+1, 0, 100));
		// HeadPhone VOLUME
		else if ( input[VOLDOWN] && !input.isActive(MODIFIER) )
			setVolume( constrain( volumeScalerPhones-1, 0, 100));
		else if ( input[VOLUP] && !input.isActive(MODIFIER) )
			setVolume( constrain( volumeScalerPhones+1, 0, 100));
		// SELLINKAPP SELECTED
		else if (menu->selLinkApp()!=NULL) {
			if ( input[MANUAL] ) menu->selLinkApp()->showManual();
		}
#else
		// SELLINKAPP SELECTED
		else if (menu->selLinkApp()!=NULL) {
			if ( input[MANUAL] ) menu->selLinkApp()->showManual();
			else if ( input.isActive(MODIFIER) ) {
				// VOLUME
				if ( input[VOLDOWN] && !input.isActive(VOLUP) )
					menu->selLinkApp()->setVolume( constrain(menu->selLinkApp()->volume()-1,0,100) );
				if ( input[VOLUP] && !input.isActive(VOLDOWN) )
					menu->selLinkApp()->setVolume( constrain(menu->selLinkApp()->volume()+1,0,100) );;
				if ( input.isActive(VOLUP) && input.isActive(VOLDOWN) ) menu->selLinkApp()->setVolume(-1);
			} else {
				// CLOCK
				if ( input[VOLDOWN] && !input.isActive(VOLUP) )
					menu->selLinkApp()->setClock( constrain(menu->selLinkApp()->clock()-10,50,confInt["maxClock"]) );
				if ( input[VOLUP] && !input.isActive(VOLDOWN) )
					menu->selLinkApp()->setClock( constrain(menu->selLinkApp()->clock()+10,50,confInt["maxClock"]) );
				if ( input.isActive(VOLUP) && input.isActive(VOLDOWN) ) menu->selLinkApp()->setClock(DEFAULT_CPU_CLK);
			}
		}
#endif
		if ( input.isActive(MODIFIER) ) {
			if (input.isActive(SECTION_PREV) && input.isActive(SECTION_NEXT))
				saveScreenshot();
		} else {
			// SECTIONS
			if ( input[SECTION_PREV] ) {
				menu->decSectionIndex();
				offset = menu->sectionLinks()->size()>linksPerPage ? 2 : 6;
			} else if ( input[SECTION_NEXT] ) {
				menu->incSectionIndex();
				offset = menu->sectionLinks()->size()>linksPerPage ? 2 : 6;
			}
		}

		if(nloops++ > 200){  // about every ten seconds
			/* Do stuff here that should be checked every once in a while:
			 * cpu speed, battery level, wifi level, clock, anything else
			 */
			nbattlevel = getBatteryLevel();
#if defined(TARGET_Z2) || defined(TARGET_IZ2S)
			nwifilevel = getWiFiLevel();
#endif
#ifdef TARGET_Z2
			nMHz = getCPUspeed();
			getTime(strTime, sizeof(strTime));
#endif
			bRedraw = true;
			nloops=0;
		}
		usleep(LOOP_DELAY);
	}

	delete btnContextMenu;
	btnContextMenu = NULL;

}

void GMenu2X::explorer() {
#ifdef TARGET_Z2
	FileDialog fd(this,tr["Select an application"],"");
#else
	FileDialog fd(this,tr["Select an application"],".gpu,.gpe,.sh");
#endif
	if (fd.exec()) {
		if (confInt["saveSelection"] && (confInt["section"]!=menu->selSectionIndex() || confInt["link"]!=menu->selLinkIndex()))
			writeConfig();
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
#else
		if (fwType == "open2x" && savedVolumeMode != volumeMode)
			writeConfigOpen2x();
#endif
		//string command = cmdclean(fd.path()+"/"+fd.file) + "; sync & cd "+cmdclean(getExePath())+"; exec ./gmenu2x";
#ifdef TARGET_IZ2S
		// Trap INT and TERM signals or they will kill this wrapper before the return to gmenu. 
		string command = "trap - INT TERM; " + cmdclean(fd.getPath()+"/"+fd.getFile());
		string filepath = progpath; //gmenu2x->getExePath();

		// Force uclibc to end the suspended parent process with a killall.
		command += "; sync & cd "+ cmdclean(filepath) +"; killall -9 gmenu2x; exec ./gmenu2x";
#else
		string command = cmdclean(fd.getPath()+"/"+fd.getFile());
#endif
		chdir(fd.getPath().c_str());
		quit();
		setClock(DEFAULT_CPU_CLK);
#ifdef TARGET_Z2
		writePID();
#endif
		execlp("/bin/sh","/bin/sh","-c",command.c_str(),NULL);
		//if execution continues then something went wrong and as we already called SDL_Quit we cannot continue
		//try relaunching gmenu2x
		WARNING("Error executing selected application, re-launching gmenu2x");
		chdir(getExePath().c_str());
		execlp("./gmenu2x", "./gmenu2x", NULL);
	}
}

void GMenu2X::options() {
	int curMenuClock = confInt["menuClock"];
	int curGlobalVolume = confInt["globalVolume"];
	//G
	int prevgamma = confInt["gamma"];
	bool showRootFolder = fileExists("/mnt/root");

	FileLister fl_tr("translations");
	fl_tr.browse();
	fl_tr.insertFile("English");
	string lang = tr.lang();

	vector<string> encodings;
	encodings.push_back("NTSC");
	encodings.push_back("PAL");

	SettingsDialog sd(this, input, ts, tr["Settings"]);
	sd.addSetting(new MenuSettingMultiString(this,tr["Language"],tr["Set the language used by GMenu2X"],&lang,&fl_tr.getFiles()));
	sd.addSetting(new MenuSettingBool(this,tr["Save last selection"],tr["Save the last selected link and section on exit"],&confInt["saveSelection"]));
#ifdef TARGET_GP2X
	sd.addSetting(new MenuSettingInt(this,tr["Clock for GMenu2X"],tr["Set the cpu working frequency when running GMenu2X"],&confInt["menuClock"],50,325));
	sd.addSetting(new MenuSettingInt(this,tr["Maximum overclock"],tr["Set the maximum overclock for launching links"],&confInt["maxClock"],50,325));
#endif
#if defined(TARGET_WIZ) || defined(TARGET_CAANOO)
	sd.addSetting(new MenuSettingInt(this,tr["Clock for GMenu2X"],tr["Set the cpu working frequency when running GMenu2X"],&confInt["menuClock"],50,900,10));
	sd.addSetting(new MenuSettingInt(this,tr["Maximum overclock"],tr["Set the maximum overclock for launching links"],&confInt["maxClock"],50,900,10));
#endif
	sd.addSetting(new MenuSettingInt(this,tr["Global Volume"],tr["Set the default volume for the gp2x soundcard"],&confInt["globalVolume"],0,100));
	sd.addSetting(new MenuSettingBool(this,tr["Output logs"],tr["Logs the output of the links. Use the Log Viewer to read them."],&confInt["outputLogs"]));
	//G
	sd.addSetting(new MenuSettingInt(this,tr["Gamma"],tr["Set gp2x gamma value (default: 10)"],&confInt["gamma"],1,100));
	sd.addSetting(new MenuSettingMultiString(this,tr["Tv-Out encoding"],tr["Encoding of the tv-out signal"],&confStr["tvoutEncoding"],&encodings));
	sd.addSetting(new MenuSettingBool(this,tr["Show root"],tr["Show root folder in the file selection dialogs"],&showRootFolder));

	if (sd.exec() && sd.edited()) {
		//G
		if (prevgamma != confInt["gamma"]) setGamma(confInt["gamma"]);
		if (curMenuClock!=confInt["menuClock"]) setClock(confInt["menuClock"]);
		if (curGlobalVolume!=confInt["globalVolume"]) setVolume(confInt["globalVolume"]);
		if (lang == "English") lang = "";
		if (lang != tr.lang()) tr.setLang(lang);
		if (fileExists("/mnt/root") && !showRootFolder)
			unlink("/mnt/root");
		else if (!fileExists("/mnt/root") && showRootFolder)
			symlink("/","/mnt/root");
		writeConfig();
	}
}

void GMenu2X::settingsOpen2x() {
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
#else
	SettingsDialog sd(this, input, ts, tr["Open2x Settings"]);
	sd.addSetting(new MenuSettingBool(this,tr["USB net on boot"],tr["Allow USB networking to be started at boot time"],&o2x_usb_net_on_boot));
	sd.addSetting(new MenuSettingString(this,tr["USB net IP"],tr["IP address to be used for USB networking"],&o2x_usb_net_ip));
	sd.addSetting(new MenuSettingBool(this,tr["Telnet on boot"],tr["Allow telnet to be started at boot time"],&o2x_telnet_on_boot));
	sd.addSetting(new MenuSettingBool(this,tr["FTP on boot"],tr["Allow FTP to be started at boot time"],&o2x_ftp_on_boot));
	sd.addSetting(new MenuSettingBool(this,tr["GP2XJOY on boot"],tr["Create a js0 device for GP2X controls"],&o2x_gp2xjoy_on_boot));
	sd.addSetting(new MenuSettingBool(this,tr["USB host on boot"],tr["Allow USB host to be started at boot time"],&o2x_usb_host_on_boot));
	sd.addSetting(new MenuSettingBool(this,tr["USB HID on boot"],tr["Allow USB HID to be started at boot time"],&o2x_usb_hid_on_boot));
	sd.addSetting(new MenuSettingBool(this,tr["USB storage on boot"],tr["Allow USB storage to be started at boot time"],&o2x_usb_storage_on_boot));
	//sd.addSetting(new MenuSettingInt(this,tr["Speaker Mode on boot"],tr["Set Speaker mode. 0 = Mute, 1 = Phones, 2 = Speaker"],&volumeMode,0,2,1));
	sd.addSetting(new MenuSettingInt(this,tr["Speaker Scaler"],tr["Set the Speaker Mode scaling 0-150\% (default is 100\%)"],&volumeScalerNormal,0,150));
	sd.addSetting(new MenuSettingInt(this,tr["Headphones Scaler"],tr["Set the Headphones Mode scaling 0-100\% (default is 65\%)"],&volumeScalerPhones,0,100));

	if (sd.exec() && sd.edited()) {
		writeConfigOpen2x();
		switch(volumeMode) {
			case VOLUME_MODE_MUTE:   setVolumeScaler(VOLUME_SCALER_MUTE); break;
			case VOLUME_MODE_PHONES: setVolumeScaler(volumeScalerPhones);   break;
			case VOLUME_MODE_NORMAL: setVolumeScaler(volumeScalerNormal); break;
		}
		setVolume(confInt["globalVolume"]);
	}
#endif
}

void GMenu2X::skinMenu() {
	FileLister fl_sk("skins",true,false);
	fl_sk.addExclude("..");
	fl_sk.browse();
	string curSkin = confStr["skin"];

	SettingsDialog sd(this, input, ts, tr["Skin"]);
	sd.addSetting(new MenuSettingMultiString(this,tr["Skin"],tr["Set the skin used by GMenu2X"],&confStr["skin"],&fl_sk.getDirectories()));
	sd.addSetting(new MenuSettingRGBA(this,tr["Top Bar Color"],tr["Color of the top bar"],&skinConfColors[COLOR_TOP_BAR_BG]));
	sd.addSetting(new MenuSettingRGBA(this,tr["Bottom Bar Color"],tr["Color of the bottom bar"],&skinConfColors[COLOR_BOTTOM_BAR_BG]));
	sd.addSetting(new MenuSettingRGBA(this,tr["Selection Color"],tr["Color of the selection and other interface details"],&skinConfColors[COLOR_SELECTION_BG]));
	sd.addSetting(new MenuSettingRGBA(this,tr["Message Box Color"],tr["Background color of the message box"],&skinConfColors[COLOR_MESSAGE_BOX_BG]));
	sd.addSetting(new MenuSettingRGBA(this,tr["Message Box Border Color"],tr["Border color of the message box"],&skinConfColors[COLOR_MESSAGE_BOX_BORDER]));
	sd.addSetting(new MenuSettingRGBA(this,tr["Message Box Selection Color"],tr["Color of the selection of the message box"],&skinConfColors[COLOR_MESSAGE_BOX_SELECTION]));
	sd.addSetting(new MenuSettingRGBA(this,tr["Font Color"],tr["Color of the font"],&skinConfColors[COLOR_FONT]));
	sd.addSetting(new MenuSettingRGBA(this,tr["Font Outline Color"],tr["Color of the font's outline"],&skinConfColors[COLOR_FONT_OUTLINE]));

	if (sd.exec() && sd.edited()) {
		if (curSkin != confStr["skin"]) {
			setSkin(confStr["skin"]);
			writeConfig();
		}
		font->setColor(skinConfColors[COLOR_FONT])->setOutlineColor(skinConfColors[COLOR_FONT_OUTLINE]);
		writeSkinConfig();
		initBG();
	}
}


void GMenu2X::toggleTvOut() {
#ifdef TARGET_GP2X
	if (cx25874!=0)
		gp2x_tvout_off();
	else
		gp2x_tvout_on(confStr["tvoutEncoding"] == "PAL");
#endif
}

void GMenu2X::setSkin(const string &skin, bool setWallpaper) {
	confStr["skin"] = skin;

	//Clear previous skin settings
	skinConfStr.clear();
	skinConfInt.clear();

	//clear collection and change the skin path
	sc.clear();
	sc.setSkin(skin);
	if (btnContextMenu != NULL)
		btnContextMenu->setIcon( btnContextMenu->getIcon() );

	//reset colors to the default values
	skinConfColors[COLOR_TOP_BAR_BG] = (RGBAColor){255,255,255,130};
	skinConfColors[COLOR_BOTTOM_BAR_BG] = (RGBAColor){255,255,255,130};
	skinConfColors[COLOR_SELECTION_BG] = (RGBAColor){255,255,255,130};
	skinConfColors[COLOR_MESSAGE_BOX_BG] = (RGBAColor){255,255,255,255};
	skinConfColors[COLOR_MESSAGE_BOX_BORDER] = (RGBAColor){80,80,80,255};
	skinConfColors[COLOR_MESSAGE_BOX_SELECTION] = (RGBAColor){160,160,160,255};
	skinConfColors[COLOR_FONT] = (RGBAColor){255,255,255,255};
	skinConfColors[COLOR_FONT_OUTLINE] = (RGBAColor){0,0,0,130};

	//load skin settings
	string skinconfname = "skins/"+skin+"/skin.conf";
	if (fileExists(skinconfname)) {
		ifstream skinconf(skinconfname.c_str(), ios_base::in);
		if (skinconf.is_open()) {
			string line;
			while (getline(skinconf, line, '\n')) {
				line = trim(line);
				DEBUG("skinconf: '%s'", line.c_str());
				string::size_type pos = line.find("=");
				string name = trim(line.substr(0,pos));
				string value = trim(line.substr(pos+1,line.length()));

				if (value.length()>0) {
					if (value.length()>1 && value.at(0)=='"' && value.at(value.length()-1)=='"')
						skinConfStr[name] = value.substr(1,value.length()-2);
					else if (value.at(0) == '#')
						skinConfColors[stringToColor(name)] = strtorgba( value.substr(1,value.length()) );
					else
						skinConfInt[name] = atoi(value.c_str());
				}
			}
			skinconf.close();

			if (setWallpaper && !skinConfStr["wallpaper"].empty() && fileExists("skins/"+skin+"/wallpapers/"+skinConfStr["wallpaper"]))
				confStr["wallpaper"] = "skins/"+skin+"/wallpapers/"+skinConfStr["wallpaper"];
		}
	}

	evalIntConf( &skinConfInt["topBarHeight"], 40, 32,120 );
	evalIntConf( &skinConfInt["linkHeight"], 40, 32,120 );
	evalIntConf( &skinConfInt["linkWidth"], 60, 32,120 );
	evalIntConf( &skinConfInt["fontSize"], 9, 6,60 );

	//recalculate some coordinates based on the new element sizes
	linkColumns = (resX-10)/skinConfInt["linkWidth"];
	linkRows = (resY-35-skinConfInt["topBarHeight"])/skinConfInt["linkHeight"];

	if (menu != NULL) menu->loadIcons();

	//Selection png
	useSelectionPng = sc.addSkinRes("imgs/selection.png") != NULL;

	//font
	initFont();
}

#ifdef TARGET_Z2
/* Saves gmenu pid */
void GMenu2X::writePID() {
	ofstream inf("/tmp/run/gmenu.pid");
	if (inf.is_open()) {
		inf << getpid()<< endl;
		inf.close();
		sync();
	}
}
#endif

#ifdef ZIPIT_Z2 // Dont bother with R and L buttons on title bar
#else
void GMenu2X::activateSdUsb() {
	if (usbnet) {
		MessageBox mb(this,tr["Operation not permitted."]+"\n"+tr["You should disable Usb Networking to do this."]);
		mb.exec();
	} else {
		system("scripts/usbon.sh sd");
		MessageBox mb(this,tr["USB Enabled (SD)"],"icons/usb.png");
		mb.setButton(CONFIRM, tr["Turn off"]);
		mb.exec();
		system("scripts/usboff.sh sd");
	}
}

void GMenu2X::activateNandUsb() {
	if (usbnet) {
		MessageBox mb(this,tr["Operation not permitted."]+"\n"+tr["You should disable Usb Networking to do this."]);
		mb.exec();
	} else {
		system("scripts/usbon.sh nand");
		MessageBox mb(this,tr["USB Enabled (Nand)"],"icons/usb.png");
		mb.setButton(CONFIRM, tr["Turn off"]);
		mb.exec();
		system("scripts/usboff.sh nand");
	}
}

void GMenu2X::activateRootUsb() {
	if (usbnet) {
		MessageBox mb(this,tr["Operation not permitted."]+"\n"+tr["You should disable Usb Networking to do this."]);
		mb.exec();
	} else {
		system("scripts/usbon.sh root");
		MessageBox mb(this,tr["USB Enabled (Root)"],"icons/usb.png");
		mb.setButton(CONFIRM, tr["Turn off"]);
		mb.exec();
		system("scripts/usboff.sh root");
	}
}
#endif

int GMenu2X::listbox(vector<MenuOption>* voices){
	bool close = false;
	uint i, sel=0, fadeAlpha=0;

	int h = font->getHeight();
	SDL_Rect box;
	box.h = (h+2)*voices->size()+8;
	box.w = 0;
	for (i=0; i<voices->size(); i++) {
		int w = font->getTextWidth(voices->at(i).text);
		if (w>box.w) box.w = w;
	}
	box.w += 23;
	box.x = halfX - box.w/2;
	box.y = halfY - box.h/2;

	SDL_Rect selbox = {box.x+4, 0, box.w-8, h+2};
	long tickNow, tickStart = SDL_GetTicks();

	Surface bg(s);
	/*//Darken background
	bg.box(0, 0, resX, resY, 0,0,0,150);
	bg.box(box.x, box.y, box.w, box.h, skinConfColors[COLOR_MESSAGE_BOX_BG]);
	bg.rectangle( box.x+2, box.y+2, box.w-4, box.h-4, skinConfColors[COLOR_MESSAGE_BOX_BORDER] );*/

	while (!close) {
		tickNow = SDL_GetTicks();

		selbox.y = box.y+4+(h+2)*sel;
		bg.blit(s,0,0);

		if (fadeAlpha<200) fadeAlpha = intTransition(0,200,tickStart,500,tickNow);
		s->box(0, 0, resX, resY, 0,0,0,fadeAlpha);
		s->box(box.x, box.y, box.w, box.h, skinConfColors[COLOR_MESSAGE_BOX_BG]);
		s->rectangle( box.x+2, box.y+2, box.w-4, box.h-4, skinConfColors[COLOR_MESSAGE_BOX_BORDER] );


		//draw selection rect
		s->box( selbox.x, selbox.y, selbox.w, selbox.h, skinConfColors[COLOR_MESSAGE_BOX_SELECTION] );
		for (i=0; i<voices->size(); i++)
			s->write( font, voices->at(i).text, box.x+12, box.y+5+(h+2)*i, HAlignLeft, VAlignTop );
		s->flip();

#ifdef TOUCHSCREEN
		//touchscreen
		if (f200) {
			ts.poll();
			if (ts.released()) {
				if (!ts.inRect(box))
					close = true;
				else if (ts.getX() >= selbox.x
					  && ts.getX() <= selbox.x + selbox.w)
					for (i=0; i<voices->size(); i++) {
						selbox.y = box.y+4+(h+2)*i;
						if (ts.getY() >= selbox.y
						 && ts.getY() <= selbox.y + selbox.h) {
							voices->at(i).action();
							close = true;
							i = voices->size();
						}
					}
			} else if (ts.pressed() && ts.inRect(box)) {
				for (i=0; i<voices->size(); i++) {
					selbox.y = box.y+4+(h+2)*i;
					if (ts.getY() >= selbox.y
					 && ts.getY() <= selbox.y + selbox.h) {
						sel = i;
						i = voices->size();
					}
				}
			}
		}
#endif
		input.update();
		if ( input[CANCEL]  ) { close = true; sel = -1; }
		if ( input[MENU]    ) { close = true; sel = -1; }
		if ( input[UP]      ) sel = max(0, sel-1);
		if ( input[DOWN]    ) sel = min((int)voices->size()-1, sel+1);
		if ( input[CONFIRM] ) { voices->at(sel).action(); close = true; }
	}
	return sel;
}

void GMenu2X::contextMenu() {
	vector<MenuOption> voices;
	{
	MenuOption opt = {tr.translate("Add link in $1",menu->selSection().c_str(),NULL), MakeDelegate(this, &GMenu2X::addLink)};
	voices.push_back(opt);
	}

	if (menu->selLinkApp()!=NULL) {
		{
		MenuOption opt = {tr.translate("Edit $1",menu->selLink()->getTitle().c_str(),NULL), MakeDelegate(this, &GMenu2X::editLink)};
		voices.push_back(opt);
		}{
		MenuOption opt = {tr.translate("Delete $1 link",menu->selLink()->getTitle().c_str(),NULL), MakeDelegate(this, &GMenu2X::deleteLink)};
		voices.push_back(opt);
		}
	}

	{
	MenuOption opt = {tr["Add section"], MakeDelegate(this, &GMenu2X::addSection)};
	voices.push_back(opt);
	}{
	MenuOption opt = {tr["Rename section"], MakeDelegate(this, &GMenu2X::renameSection)};
	voices.push_back(opt);
	}{
	MenuOption opt = {tr["Delete section"], MakeDelegate(this, &GMenu2X::deleteSection)};
	voices.push_back(opt);
	}{
	MenuOption opt = {tr["Scan for applications and games"], MakeDelegate(this, &GMenu2X::scanner)};
	voices.push_back(opt);
	}

	bool close = false;
	uint i, sel=0, fadeAlpha=0;

	int h = font->getHeight();
	int h2 = font->getHalfHeight();
	SDL_Rect box;
	box.h = (h+2)*voices.size()+8;
	box.w = 0;
	for (i=0; i<voices.size(); i++) {
		int w = font->getTextWidth(voices[i].text);
		if (w>box.w) box.w = w;
	}
	box.w += 23;
	box.x = halfX - box.w/2;
	box.y = halfY - box.h/2;

	SDL_Rect selbox = {box.x+4, 0, box.w-8, h+2};
	long tickNow, tickStart = SDL_GetTicks();

	Surface bg(s);
	/*//Darken background
	bg.box(0, 0, resX, resY, 0,0,0,150);
	bg.box(box.x, box.y, box.w, box.h, skinConfColors[COLOR_MESSAGE_BOX_BG]);
	bg.rectangle( box.x+2, box.y+2, box.w-4, box.h-4, skinConfColors[COLOR_MESSAGE_BOX_BORDER] );*/
	while (!close) {
		tickNow = SDL_GetTicks();

		selbox.y = box.y+4+(h+2)*sel;
		bg.blit(s,0,0);

		if (fadeAlpha<200) fadeAlpha = intTransition(0,200,tickStart,500,tickNow);
		s->box(0, 0, resX, resY, 0,0,0,fadeAlpha);
		s->box(box.x, box.y, box.w, box.h, skinConfColors[COLOR_MESSAGE_BOX_BG]);
		s->rectangle( box.x+2, box.y+2, box.w-4, box.h-4, skinConfColors[COLOR_MESSAGE_BOX_BORDER] );


		//draw selection rect
		s->box( selbox.x, selbox.y, selbox.w, selbox.h, skinConfColors[COLOR_MESSAGE_BOX_SELECTION] );
		for (i=0; i<voices.size(); i++)
			s->write( font, voices[i].text, box.x+12, box.y+h2+5+(h+2)*i, HAlignLeft, VAlignMiddle );
		s->flip();

#ifdef TOUCHSCREEN
		//touchscreen
		if (f200) {
			ts.poll();
			if (ts.released()) {
				if (!ts.inRect(box))
					close = true;
				else if (ts.getX() >= selbox.x
					  && ts.getX() <= selbox.x + selbox.w)
					for (i=0; i<voices.size(); i++) {
						selbox.y = box.y+4+(h+2)*i;
						if (ts.getY() >= selbox.y
						 && ts.getY() <= selbox.y + selbox.h) {
							voices[i].action();
							close = true;
							i = voices.size();
						}
					}
			} else if (ts.pressed() && ts.inRect(box)) {
				for (i=0; i<voices.size(); i++) {
					selbox.y = box.y+4+(h+2)*i;
					if (ts.getY() >= selbox.y
					 && ts.getY() <= selbox.y + selbox.h) {
						sel = i;
						i = voices.size();
					}
				}
			}
		}
#endif
		input.update();
		if ( input[CANCEL]  ) close = true;
		if ( input[MENU]    ) close = true;
		if ( input[UP]      ) sel = max(0, sel-1);
		if ( input[DOWN]    ) sel = min((int)voices.size()-1, sel+1);
		if ( input[CONFIRM] ) { voices[sel].action(); close = true; }
	}
}

void GMenu2X::changeWallpaper() {
	WallpaperDialog wp(this);
	if (wp.exec() && confStr["wallpaper"] != wp.wallpaper) {
		confStr["wallpaper"] = wp.wallpaper;
		initBG();
		writeConfig();
	}
}

void GMenu2X::saveScreenshot() {
	ledOn();
	uint x = 0;
	stringstream ss;
	string fname;
	do {
		x++;
		fname = "";
		ss.clear();
		ss << x;
		ss >> fname;
		fname = "screen"+fname+".bmp";
	} while (fileExists(fname));
	SDL_SaveBMP(s->raw,fname.c_str());
	sync();
	ledOff();
}

void GMenu2X::addLink() {
	FileDialog fd(this,tr["Select an application"]);
	if (fd.exec()) {
		ledOn();
		menu->addLink(fd.getPath(), fd.getFile());
		sync();
		ledOff();
	}
}

void GMenu2X::editLink() {
	if (menu->selLinkApp()==NULL) return;

	vector<string> pathV;
	split(pathV,menu->selLinkApp()->getFile(),"/");
	string oldSection = "";
	if (pathV.size()>1)
		oldSection = pathV[pathV.size()-2];
	string newSection = oldSection;

	string linkTitle = menu->selLinkApp()->getTitle();
	string linkDescription = menu->selLinkApp()->getDescription();
	string linkIcon = menu->selLinkApp()->getIcon();
	string linkManual = menu->selLinkApp()->getManual();
	string linkParams = menu->selLinkApp()->getParams();
	string linkSelFilter = menu->selLinkApp()->getSelectorFilter();
	string linkSelDir = menu->selLinkApp()->getSelectorDir();
	bool linkSelBrowser = menu->selLinkApp()->getSelectorBrowser();
	bool linkUseRamTimings = menu->selLinkApp()->getUseRamTimings();
	string linkSelScreens = menu->selLinkApp()->getSelectorScreens();
	string linkSelAliases = menu->selLinkApp()->getAliasFile();
	int linkClock = menu->selLinkApp()->clock();
	int linkVolume = menu->selLinkApp()->volume();
	//G
	int linkGamma = menu->selLinkApp()->gamma();

	string diagTitle = tr.translate("Edit link: $1",linkTitle.c_str(),NULL);
	string diagIcon = menu->selLinkApp()->getIconPath();

	string strClock;
	stringstream ss;
	ss << DEFAULT_CPU_CLK;
	ss >> strClock;
	const string wd = menu->selLinkApp()->getRealWorkdir();

	SettingsDialog sd(this, input, ts, diagTitle, diagIcon);
	sd.addSetting(new MenuSettingString(      this, tr["Title"],                tr["Link title"], &linkTitle, diagTitle, diagIcon ));
	sd.addSetting(new MenuSettingString(      this, tr["Description"],          tr["Link description"], &linkDescription, diagTitle, diagIcon ));
	sd.addSetting(new MenuSettingMultiString( this, tr["Section"],              tr["The section this link belongs to"], &newSection, &menu->getSections() ));
	sd.addSetting(new MenuSettingImage(       this, tr["Icon"],                 tr.translate("Select an icon for the link: $1", linkTitle.c_str(), NULL), &linkIcon, ".png,.bmp,.jpg,.jpeg", wd ));
	sd.addSetting(new MenuSettingFile(        this, tr["Manual"],               tr["Select a graphic/textual manual or a readme"], &linkManual, ".man.png,.txt", wd ));
	sd.addSetting(new MenuSettingInt(         this, tr.translate("Clock (default: $1)",strClock.c_str(), NULL), tr["Cpu clock frequency to set when launching this link"], &linkClock, 50, confInt["maxClock"] ));
	sd.addSetting(new MenuSettingBool(        this, tr["Tweak RAM Timings"],    tr["This usually speeds up the application at the cost of stability"], &linkUseRamTimings ));
	sd.addSetting(new MenuSettingInt(         this, tr["Volume (default: -1)"], tr["Volume to set for this link"], &linkVolume, -1, 100 ));
	sd.addSetting(new MenuSettingString(      this, tr["Parameters"],           tr["Parameters to pass to the application"], &linkParams, diagTitle, diagIcon ));
	sd.addSetting(new MenuSettingDir(         this, tr["Selector Directory"],   tr["Directory to scan for the selector"], &linkSelDir, wd ));
	sd.addSetting(new MenuSettingBool(        this, tr["Selector Browser"],     tr["Allow the selector to change directory"], &linkSelBrowser ));
	sd.addSetting(new MenuSettingString(      this, tr["Selector Filter"],      tr["Filter for the selector (Separate values with a comma)"], &linkSelFilter, diagTitle, diagIcon ));
	sd.addSetting(new MenuSettingDir(         this, tr["Selector Screenshots"], tr["Directory of the screenshots for the selector"], &linkSelScreens, wd ));
	sd.addSetting(new MenuSettingFile(        this, tr["Selector Aliases"],     tr["File containing a list of aliases for the selector"], &linkSelAliases, wd ));
	//G
	sd.addSetting(new MenuSettingInt(         this, tr["Gamma (default: 0)"],   tr["Gamma value to set when launching this link"], &linkGamma, 0, 100 ));
	sd.addSetting(new MenuSettingBool(        this, tr["Wrapper"],              tr["Explicitly relaunch GMenu2X after this link's execution ends"], &menu->selLinkApp()->needsWrapperRef() ));
	sd.addSetting(new MenuSettingBool(        this, tr["Don't Leave"],          tr["Don't quit GMenu2X when launching this link"], &menu->selLinkApp()->runsInBackgroundRef() ));

	if (sd.exec() && sd.edited()) {
		ledOn();

		menu->selLinkApp()->setTitle(linkTitle);
		menu->selLinkApp()->setDescription(linkDescription);
		menu->selLinkApp()->setIcon(linkIcon);
		menu->selLinkApp()->setManual(linkManual);
		menu->selLinkApp()->setParams(linkParams);
		menu->selLinkApp()->setSelectorFilter(linkSelFilter);
		menu->selLinkApp()->setSelectorDir(linkSelDir);
		menu->selLinkApp()->setSelectorBrowser(linkSelBrowser);
		menu->selLinkApp()->setUseRamTimings(linkUseRamTimings);
		menu->selLinkApp()->setSelectorScreens(linkSelScreens);
		menu->selLinkApp()->setAliasFile(linkSelAliases);
		menu->selLinkApp()->setClock(linkClock);
		menu->selLinkApp()->setVolume(linkVolume);
		//G
		menu->selLinkApp()->setGamma(linkGamma);

		INFO("New Section: '%s'", newSection.c_str());

		//if section changed move file and update link->file
		if (oldSection!=newSection) {
			vector<string>::const_iterator newSectionIndex = find(menu->getSections().begin(),menu->getSections().end(),newSection);
			if (newSectionIndex==menu->getSections().end()) return;
			string newFileName = "sections/"+newSection+"/"+linkTitle;
			uint x=2;
			while (fileExists(newFileName)) {
				string id = "";
				stringstream ss; ss << x; ss >> id;
				newFileName = "sections/"+newSection+"/"+linkTitle+id;
				x++;
			}
			rename(menu->selLinkApp()->getFile().c_str(),newFileName.c_str());
			menu->selLinkApp()->renameFile(newFileName);

			INFO("New section index: %i.", newSectionIndex - menu->getSections().begin());

			menu->linkChangeSection(menu->selLinkIndex(), menu->selSectionIndex(), newSectionIndex - menu->getSections().begin());
		}
		menu->selLinkApp()->save();
		sync();

		ledOff();
	}
}

void GMenu2X::deleteLink() {
	if (menu->selLinkApp()!=NULL) {
		MessageBox mb(this, tr.translate("Deleting $1",menu->selLink()->getTitle().c_str(),NULL)+"\n"+tr["Are you sure?"], menu->selLink()->getIconPath());
		mb.setButton(CONFIRM, tr["Yes"]);
		mb.setButton(CANCEL,  tr["No"]);
		if (mb.exec() == CONFIRM) {
			ledOn();
			menu->deleteSelectedLink();
			sync();
			ledOff();
		}
	}
}

void GMenu2X::addSection() {
	InputDialog id(this, input, ts, tr["Insert a name for the new section"]);
	if (id.exec()) {
		//only if a section with the same name does not exist
		if (find(menu->getSections().begin(), menu->getSections().end(), id.getInput())
				== menu->getSections().end()) {
			//section directory doesn't exists
			ledOn();
			if (menu->addSection(id.getInput())) {
				menu->setSectionIndex( menu->getSections().size()-1 ); //switch to the new section
				sync();
			}
			ledOff();
		}
	}
}

void GMenu2X::renameSection() {
	InputDialog id(this, input, ts, tr["Insert a new name for this section"],menu->selSection());
	if (id.exec()) {
		//only if a section with the same name does not exist & !samename
		if (menu->selSection() != id.getInput()
		 && find(menu->getSections().begin(),menu->getSections().end(), id.getInput())
				== menu->getSections().end()) {
			//section directory doesn't exists
			string newsectiondir = "sections/" + id.getInput();
			string sectiondir = "sections/" + menu->selSection();
			ledOn();
			if (rename(sectiondir.c_str(), "tmpsection")==0 && rename("tmpsection", newsectiondir.c_str())==0) {
				string oldpng = sectiondir+".png", newpng = newsectiondir+".png";
				string oldicon = sc.getSkinFilePath(oldpng), newicon = sc.getSkinFilePath(newpng);
				if (!oldicon.empty() && newicon.empty()) {
					newicon = oldicon;
          				newicon.replace(newicon.find(oldpng), oldpng.length(), newpng);

					if (!fileExists(newicon)) {
						rename(oldicon.c_str(), "tmpsectionicon");
						rename("tmpsectionicon", newicon.c_str());
						sc.move("skin:"+oldpng, "skin:"+newpng);
					}
				}
				menu->renameSection(menu->selSectionIndex(), id.getInput());
				sync();
			}
			ledOff();
		}
	}
}

void GMenu2X::deleteSection() {
	MessageBox mb(this,tr["You will lose all the links in this section."]+"\n"+tr["Are you sure?"]);
	mb.setButton(CONFIRM, tr["Yes"]);
	mb.setButton(CANCEL,  tr["No"]);
	if (mb.exec() == CONFIRM) {
		ledOn();
		if (rmtree(path+"sections/"+menu->selSection())) {
			menu->deleteSelectedSection();
			sync();
		}
		ledOff();
	}
}

void GMenu2X::scanner() {
	Surface scanbg(bg);
	drawButton(&scanbg, "x", tr["Exit"],
	drawButton(&scanbg, "b", "", 5)-10);
	scanbg.write(font,tr["Link Scanner"],halfX,7,HAlignCenter,VAlignMiddle);

	uint lineY = 42;

	if (confInt["menuClock"]<DEFAULT_CPU_CLK) {
		setClock(DEFAULT_CPU_CLK);
		string strClock;
		stringstream ss;
		ss << DEFAULT_CPU_CLK;
		ss >> strClock;
		scanbg.write(font,tr.translate("Raising cpu clock to $1Mhz", strClock.c_str(), NULL),5,lineY);
		scanbg.blit(s,0,0);
		s->flip();
		lineY += 26;
	}

	scanbg.write(font,tr["Scanning SD filesystem..."],5,lineY);
	scanbg.blit(s,0,0);
	s->flip();
	lineY += 26;

	vector<string> files;
#ifdef TARGET_Z2
	scanPath(sdcard,&files);
#else
	scanPath("/mnt/sd",&files);
#endif

	//Onyl gph firmware has nand
	if (fwType=="gph" && !f200) {
		scanbg.write(font,tr["Scanning NAND filesystem..."],5,lineY);
		scanbg.blit(s,0,0);
		s->flip();
		lineY += 26;
		scanPath("/mnt/nand",&files);
	}

	stringstream ss;
	ss << files.size();
	string str = "";
	ss >> str;
	scanbg.write(font,tr.translate("$1 files found.",str.c_str(),NULL),5,lineY);
	lineY += 26;
	scanbg.write(font,tr["Creating links..."],5,lineY);
	scanbg.blit(s,0,0);
	s->flip();
	lineY += 26;

	string path, file;
	string::size_type pos;
	uint linkCount = 0;

	ledOn();
	for (uint i = 0; i<files.size(); i++) {
		pos = files[i].rfind("/");
		if (pos!=string::npos && pos>0) {
			path = files[i].substr(0, pos+1);
			file = files[i].substr(pos+1, files[i].length());
			if (menu->addLink(path,file,"found "+file.substr(file.length()-3,3)))
				linkCount++;
		}
	}

	ss.clear();
	ss << linkCount;
	ss >> str;
	scanbg.write(font,tr.translate("$1 links created.",str.c_str(),NULL),5,lineY);
	scanbg.blit(s,0,0);
	s->flip();
	lineY += 26;

	if (confInt["menuClock"]<DEFAULT_CPU_CLK) {
		setClock(confInt["menuClock"]);
		scanbg.write(font,tr["Decreasing cpu clock"],5,lineY);
		scanbg.blit(s,0,0);
		s->flip();
		lineY += 26;
	}

	sync();
	ledOff();

	bool close = false;
	while (!close) {
		input.update();
		if (input[SETTINGS] || input[CONFIRM] || input[CANCEL]) close = true;
	}
}

void GMenu2X::scanPath(string path, vector<string> *files) {
	DIR *dirp;
	struct stat st;
	struct dirent *dptr;
	string filepath, ext;

	if (path[path.length()-1]!='/') path += "/";
	if ((dirp = opendir(path.c_str())) == NULL) return;

	while ((dptr = readdir(dirp))) {
		if (dptr->d_name[0]=='.')
			continue;
		filepath = path+dptr->d_name;
		int statRet = stat(filepath.c_str(), &st);
		if (S_ISDIR(st.st_mode))
			scanPath(filepath, files);
		if (statRet != -1) {
			ext = filepath.substr(filepath.length()-4,4);
#if defined(TARGET_GP2X) || defined(TARGET_WIZ) || defined(TARGET_CAANOO) || defined(TARGET_Z2)
			if (ext==".gpu" || ext==".gpe")
#endif
				files->push_back(filepath);
		}
	}

	closedir(dirp);
}

#ifdef ZIPIT_Z2
enum POWERSTATE {
	AC_CHRG,
	AC_FULL,
	AC_DISCHRG,
	DC_POWER
};

POWERSTATE getPwrState() {

	POWERSTATE pwrstate=DC_POWER;
	FILE* acHandle = fopen("/sys/class/power_supply/Z2/status", "r");

	if (acHandle){
		char acVal[32];
		memset(acVal, 0, sizeof(acVal));
		fread(acVal, 1, sizeof(acVal), acHandle);

		if (strncmp(acVal, "Charging", strlen("Charging")) == 0)
			pwrstate=AC_CHRG;

		if (strncmp(acVal, "Full", strlen("Full")) == 0)
			pwrstate=AC_FULL;

		if (strncmp(acVal, "Discharging", strlen("Discharging")) == 0)
			pwrstate=AC_DISCHRG;

		fclose(acHandle);
	}

	return pwrstate;
}
#endif

unsigned short GMenu2X::getBatteryLevel() {
#ifdef ZIPIT_Z2
	if (getPwrState() == AC_CHRG) return 6;

	if (getPwrState() == AC_FULL) return 7;

	char line[LINE_BUFSIZE];
	vector<string> scriptOutput;

	/* Get a pipe where the output from the scripts comes in */
	FILE* pipe = popen("/usr/bin/battlevel", "r");
	if (pipe == NULL) return 6; /* return with exit code indicating error */

	/* Read script output from the pipe... for this one there should only be one line */
	while (fgets(line, LINE_BUFSIZE, pipe) != NULL) {
		scriptOutput.push_back(line);
	}

	pclose(pipe); /* Close the pipe */

	int volt_val = 0;
	if(scriptOutput.size())
		volt_val = atoi(scriptOutput[0].c_str());
	if (volt_val>90) return 5;
	else if (volt_val>70) 	return 4;
	else if (volt_val>50) 	return 3;
	else if (volt_val>30) 	return 2;
	else if (volt_val>1) 	return 1;
	else return 0;

#else
	if (batteryHandle<=0) return 6; //AC Power
#endif

#if defined(TARGET_GP2X)
	if (f200) {
		MMSP2ADC val;
		read(batteryHandle, &val, sizeof(MMSP2ADC));

		if (val.batt==0) return 5;
		if (val.batt==1) return 3;
		if (val.batt==2) return 1;
		if (val.batt==3) return 0;
		return 6;
	} else {
		int battval = 0;
		unsigned short cbv, min=900, max=0;

		for (int i = 0; i < BATTERY_READS; i ++) {
			if ( read(batteryHandle, &cbv, 2) == 2) {
				battval += cbv;
				if (cbv>max) max = cbv;
				if (cbv<min) min = cbv;
			}
		}

		battval -= min+max;
		battval /= BATTERY_READS-2;

		if (battval>=850) return 6;
		if (battval>780) return 5;
		if (battval>740) return 4;
		if (battval>700) return 3;
		if (battval>690) return 2;
		if (battval>680) return 1;
	}
#elif defined(TARGET_WIZ) || defined(TARGET_CAANOO)
	unsigned short cbv;
	if ( read(batteryHandle, &cbv, 2) == 2) {
		// 0=fail, 1=100%, 2=66%, 3=33%, 4=0%
		switch (cbv) {
			case 4: return 1;
			case 3: return 2;
			case 2: return 4;
			case 1: return 5;
			default: return 6;
		}
	}
#endif

	return 6; //AC Power
}

void GMenu2X::setInputSpeed() {
	input.setInterval(150);
	input.setInterval(30,  VOLDOWN);
	input.setInterval(30,  VOLUP);
	input.setInterval(30,  CANCEL);
	input.setInterval(500, SETTINGS);
	input.setInterval(500, MENU);
	input.setInterval(300, MANUAL);
	input.setInterval(200, INC);
	input.setInterval(200, DEC);
	input.setInterval(1000,CONFIRM);
	input.setInterval(300, SECTION_PREV);
	input.setInterval(300, SECTION_NEXT);
	input.setInterval(300, PAGEUP);
	input.setInterval(300, PAGEDOWN);
	input.setInterval(30,  WIFI_SETUP);
}

void GMenu2X::applyRamTimings() {
#ifdef TARGET_GP2X
	// 6 4 1 1 1 2 2
	if (memdev!=0) {
		int tRC = 5, tRAS = 3, tWR = 0, tMRD = 0, tRFC = 0, tRP = 1, tRCD = 1;
		memregs[0x3802>>1] = ((tMRD & 0xF) << 12) | ((tRFC & 0xF) << 8) | ((tRP & 0xF) << 4) | (tRCD & 0xF);
		memregs[0x3804>>1] = ((tRC & 0xF) << 8) | ((tRAS & 0xF) << 4) | (tWR & 0xF);
	}
#endif
}

void GMenu2X::applyDefaultTimings() {
#ifdef TARGET_GP2X
	// 8 16 3 8 8 8 8
	if (memdev!=0) {
		int tRC = 7, tRAS = 15, tWR = 2, tMRD = 7, tRFC = 7, tRP = 7, tRCD = 7;
		memregs[0x3802>>1] = ((tMRD & 0xF) << 12) | ((tRFC & 0xF) << 8) | ((tRP & 0xF) << 4) | (tRCD & 0xF);
		memregs[0x3804>>1] = ((tRC & 0xF) << 8) | ((tRAS & 0xF) << 4) | (tWR & 0xF);
	}
#endif
}

void GMenu2X::setClock(unsigned mhz) {
	mhz = constrain(mhz,50,confInt["maxClock"]);
	if (memdev > 0) {
		DEBUG("Setting clock to %d", mhz);
#ifdef TARGET_GP2X
		unsigned v;
		unsigned mdiv, pdiv=3, scale=0;

		#define SYS_CLK_FREQ 7372800
		mhz *= 1000000;
		mdiv = (mhz * pdiv) / SYS_CLK_FREQ;
		mdiv = ((mdiv-8)<<8) & 0xff00;
		pdiv = ((pdiv-2)<<2) & 0xfc;
		scale &= 3;
		v = mdiv | pdiv | scale;
		MEM_REG[0x910>>1] = v;
#elif defined(TARGET_CAANOO) || defined(TARGET_WIZ)
		volatile unsigned int *memregl = static_cast<volatile unsigned int*>((volatile void*)memregs);
		int mdiv, pdiv = 9, sdiv = 0;
		unsigned long v;

		#define SYS_CLK_FREQ 27
		#define PLLSETREG0   (memregl[0xF004>>2])
		#define PWRMODE      (memregl[0xF07C>>2])
		mdiv = (mhz * pdiv) / SYS_CLK_FREQ;
		if (mdiv & ~0x3ff) return;
		v = pdiv<<18 | mdiv<<8 | sdiv;

		PLLSETREG0 = v;
		PWRMODE |= 0x8000;
		for (int i = 0; (PWRMODE & 0x8000) && i < 0x100000; i++);
#endif
	}
}

void GMenu2X::setGamma(int gamma) {
#ifdef TARGET_GP2X
	float fgamma = (float)constrain(gamma,1,100)/10;
	fgamma = 1 / fgamma;
	MEM_REG[0x2880>>1]&=~(1<<12);
	MEM_REG[0x295C>>1]=0;

	for (int i=0; i<256; i++) {
		unsigned char g = (unsigned char)(255.0*pow(i/255.0,fgamma));
		unsigned short s = (g<<8) | g;
		MEM_REG[0x295E>>1]= s;
		MEM_REG[0x295E>>1]= g;
	}
#endif
}

int GMenu2X::getVolume() {
	int vol = -1;
	unsigned long soundDev = open("/dev/mixer", O_RDONLY);
	if (soundDev) {
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
		ioctl(soundDev, MIXER_READ(SOUND_MIXER_PHONEOUT), &vol);
		close(soundDev);
		if (vol != -1) {
			//just return one channel , not both channels, they're hopefully the same anyways
			vol = vol & 0xFF;
			volumeScalerPhones = vol;
			return vol;
		}
#else
		ioctl(soundDev, SOUND_MIXER_READ_PCM, &vol);
		close(soundDev);
		if (vol != -1) {
			//just return one channel , not both channels, they're hopefully the same anyways
			return vol & 0xFF;
		}
#endif
	}
	return vol;
}

void GMenu2X::setVolume(int vol) {
	vol = constrain(vol,0,100);
	unsigned long soundDev = open("/dev/mixer", O_RDWR);
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
	volumeScalerPhones = vol;
	if (soundDev) {
		vol = (vol << 8) | vol;
		ioctl(soundDev, MIXER_WRITE(SOUND_MIXER_PHONEOUT), &vol);
		close(soundDev);
	}
#else
	if (soundDev) {
		vol = (vol << 8) | vol;
		ioctl(soundDev, SOUND_MIXER_WRITE_PCM, &vol);
		close(soundDev);
	}
#endif
}

void GMenu2X::setVolumeScaler(int scale) {
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
	scale = constrain(scale,0,100);
	volumeScalerNormal=scale;
	unsigned long soundDev = open("/dev/mixer", O_WRONLY);
	if (soundDev) {
		ioctl(soundDev, SOUND_MIXER_WRITE_SPEAKER, &scale);
		close(soundDev);
	}
#else
	scale = constrain(scale,0,MAX_VOLUME_SCALE_FACTOR);
	unsigned long soundDev = open("/dev/mixer", O_WRONLY);
	if (soundDev) {
		ioctl(soundDev, SOUND_MIXER_PRIVATE2, &scale);
		close(soundDev);
	}
#endif
}

int GMenu2X::getVolumeScaler() {
#ifdef TARGET_Z2 /* ZIPIT_Z2_VOLUME */
	int vol = -1;
	unsigned long soundDev = open("/dev/mixer", O_RDONLY);
	if (soundDev) {
		ioctl(soundDev, SOUND_MIXER_READ_SPEAKER, &vol);
		close(soundDev);
		if (vol != -1) {
			//just return one channel , not both channels, they're hopefully the same anyways
			vol = vol & 0xFF;
			volumeScalerNormal = vol;
			return vol;
		}
	}
	return vol;
#else
	int currentscalefactor = -1;
	unsigned long soundDev = open("/dev/mixer", O_RDONLY);
	if (soundDev) {
		ioctl(soundDev, SOUND_MIXER_PRIVATE1, &currentscalefactor);
		close(soundDev);
	}
	return currentscalefactor;
#endif
}

const string &GMenu2X::getExePath() {
	if (path.empty()) {
#if defined(TARGET_IZ2S)
		// readlink returns -1 and empty buf.  uclibc bug?
		// but gmenu2x hangs if we return a good path.  WTF?
		path = "";
#else
		char buf[255];
		memset(buf, 0, 255);
		int l = readlink("/proc/self/exe", buf, 255);

		path = buf;

		path = path.substr(0,l);
		l = path.rfind("/");
		path = path.substr(0,l+1);
#endif
	}
	return path;
}

string GMenu2X::getDiskFree() {
	stringstream ss;
	string df = "";
	struct statvfs b;

#ifdef TARGET_Z2
	string units = "MB";
	string tunits = "MB";
	int ret = statvfs(sdcard, &b);
	if (ret==0) {
		// Make sure that the multiplication happens in 64 bits.
		unsigned long long total = ((unsigned long long)b.f_blocks * b.f_frsize) / 1048576;
		if ((total == 0) && (statvfs("/mnt/ffs", &b) != 0))
		  return df;
		total = ((unsigned long long)b.f_blocks * b.f_frsize) / 1048576;
		unsigned long long free = ((unsigned long long)b.f_bfree * b.f_bsize) / 1024;
		if(free>2097152) // Anything over 2GB show in GB.
			{ free /= 1048576; units="GB"; }
		else if(free>1024) // MB
			free /= 1024; 
		else
			units="KB";
		if(total>2048) // Anything over 2GB show in GB.
			{ total /= 1024; tunits="GB"; }
		if ((units == tunits) || (free == 0))
		  units = "";
		ss << free << units << "/" << total << tunits;
		ss >> df;
	} else {WARNING("statvfs failed with error '%s'.\n", strerror(errno));}
#else
	int ret = statvfs("/mnt/sd", &b);
	if (ret==0) {
		unsigned long long free = (unsigned long long)(((unsigned long long)b.f_bfree * b.f_bsize) / 1048576.0);
		unsigned long long total = (unsigned long long)(((unsigned long long)b.f_blocks * b.f_frsize) / 1048576.0);
		ss << free << "/" << total << "MB";
		ss >> df;
	} else WARNING("statvfs failed with error '%s'.", strerror(errno));
#endif
	return df;
}

int GMenu2X::drawButton(Button *btn, int x, int y) {
	if (y<0) y = resY+y;
	btn->setPosition(x, y-7);
	btn->paint();
	return x+btn->getRect().w+6;
}

int GMenu2X::drawButton(Surface *s, const string &btn, const string &text, int x, int y) {
	if (y<0) y = resY+y;
	SDL_Rect re = {x, y-7, 0, 16};
	if (sc.skinRes("imgs/buttons/"+btn+".png") != NULL) {
		sc["imgs/buttons/"+btn+".png"]->blit(s, x, y-7);
		re.w = sc["imgs/buttons/"+btn+".png"]->raw->w+3;
		s->write(font, text, x+re.w, y, HAlignLeft, VAlignMiddle);
		re.w += font->getTextWidth(text);
	}
	return x+re.w+6;
}

int GMenu2X::drawButtonRight(Surface *s, const string &btn, const string &text, int x, int y) {
	if (y<0) y = resY+y;
	if (sc.skinRes("imgs/buttons/"+btn+".png") != NULL) {
		x -= 16;
		sc["imgs/buttons/"+btn+".png"]->blit(s, x, y-7);
		x -= 3;
		s->write(font, text, x, y, HAlignRight, VAlignMiddle);
		return x-6-font->getTextWidth(text);
	}
	return x-6;
}

void GMenu2X::drawScrollBar(uint pagesize, uint totalsize, uint pagepos, uint top, uint height) {
	if (totalsize<=pagesize) return;

	s->rectangle(resX-8, top, 7, height, skinConfColors[COLOR_SELECTION_BG]);

	//internal bar total height = height-2
	//bar size
	uint bs = (height-2) * pagesize / totalsize;
	//bar y position
	uint by = (height-2) * pagepos / totalsize;
	by = top+2+by;
	if (by+bs>top+height-2) by = top+height-2-bs;


	s->box(resX-6, by, 3, bs, skinConfColors[COLOR_SELECTION_BG]);
}

void GMenu2X::drawTopBar(Surface *s) {
	if (s==NULL) s = this->s;

	Surface *bar = sc.skinRes("imgs/topbar.png");
	if (bar != NULL)
		bar->blit(s, 0, 0);
	else
		s->box(0, 0, resX, skinConfInt["topBarHeight"],
		skinConfColors[COLOR_TOP_BAR_BG]);
}

void GMenu2X::drawBottomBar(Surface *s) {
	if (s==NULL) s = this->s;

	Surface *bar = sc.skinRes("imgs/bottombar.png");
	if (bar != NULL)
		bar->blit(s, 0, resY-bar->raw->h);
	else
		s->box(0, resY-20, resX, 20, skinConfColors[COLOR_BOTTOM_BAR_BG]);
}

unsigned short GMenu2X::getWiFiLevel() {
	int nWiFi = 0;
#ifdef TARGET_IZ2S
	char *eth0start;
	FILE *devfd;
	int noise;
	char buf[256];
	static int bufsize = 255;

	devfd = fopen("/proc/net/wireless", "r");

	// ignore the first two lines of the file
	fgets(buf, bufsize, devfd);
	fgets(buf, bufsize, devfd);

	while (fgets(buf, bufsize, devfd)) {
	  if ((eth0start = strstr(buf, "eth0:")) != NULL) {
		sscanf(eth0start + 6, "%*d %*d %d  %d", &nWiFi, &noise);
		nWiFi -= noise;
		if (nWiFi < 0)
		  nWiFi = 0;
		break;
	  }
	}
	fclose(devfd);

#endif
#ifdef TARGET_Z2
    char line[LINE_BUFSIZE];
    vector<string> scriptOutput;

    /* Get a pipe where the output from the scripts comes in */
    FILE* pipe = popen("/usr/sbin/iwconfig wlan0 | /bin/grep \"Quality\" | /bin/sed 's#.*ity=##g' | /bin/sed 's#/70.*##g'", "r");
		if (pipe == NULL) return 6; /* return with exit code indicating error */

    /* Read script output from the pipe... for this one there should only be one line */
    while (fgets(line, LINE_BUFSIZE, pipe) != NULL) {
		scriptOutput.push_back(line);
    }

	pclose(pipe); /* Close the pipe */

	if(scriptOutput.size())
		nWiFi = atoi(scriptOutput[0].c_str());
#endif

	if 		(nWiFi==0) 	return 0;
	else if (nWiFi>66) 	return 5;
	else if (nWiFi>60) 	return 4;
	else if (nWiFi>50) 	return 3;
	else if (nWiFi>30)  return 2;
	else 				return 1;
}

unsigned short GMenu2X::getCPUspeed() {
#ifdef TARGET_Z2
	FILE* mhzHandle = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
#endif

	if (!mhzHandle) return 0;
	int hz = 0;
	fscanf(mhzHandle, "%d", &hz);
	fclose(mhzHandle);
	hz /= 1000;
#ifdef TARGET_Z2
	// printf("CPUSpeed: %d\n", hz);
	if      ( hz == 156 ) return 1;
	else if ( hz == 208 ) return 2;
	else if ( hz == 312 ) return 3;
	else if ( hz == 416 ) return 4;
	else if ( hz == 520 ) return 5;
	else                 return 0;
#endif
}

void GMenu2X::wifiSetup() {
	vector<MenuOption> voices;

	{
	MenuOption opt = { tr["Connect"], MakeDelegate(this, &GMenu2X::wifiConnect) };
	voices.push_back(opt);
	}{
	MenuOption opt = { tr["Add Network"], MakeDelegate(this, &GMenu2X::wifiAddNetwork) };
	voices.push_back(opt);
	}{
	MenuOption opt = { tr["Turn Off"], MakeDelegate(this, &GMenu2X::wifiOff) };
	voices.push_back(opt);
	}

	listbox(&voices);
}

void GMenu2X::wifiOff() {
	system("/sbin/ifconfig wlan0 down");

	nwifilevel = getWiFiLevel();
	bRedraw=true;

	return;
}

void GMenu2X::wifiConnect() {
	MessageBox mb(this,tr["Connecting to wireless network..."],"skin:icons/wifi.png", MakeDelegate(this, &GMenu2X::wpaConnect));
	if(mb.exec() == 1)
		wifiAddNetwork();
}

void GMenu2X::wifiAddNetwork() {

    char line[LINE_BUFSIZE];
    vector<string> scriptOutput;
	vector<MenuOption> voices;

    FILE* pipe = popen( GMENU2X_SYSTEM_DIR "/scripts/wifi-scan", "r");
		if (pipe == NULL) return;

    while (fgets(line, LINE_BUFSIZE, pipe) != NULL)
		scriptOutput.push_back(line);

	pclose(pipe);

	if(scriptOutput.empty()){
		MessageBox mb(this,tr["No Networks were found."],"skin:icons/wifi.png");
		mb.exec();
		return;
	}

	for (unsigned int i=0; i<scriptOutput.size(); i++){
		MenuOption opt = { scriptOutput[i], MakeDelegate(this, &GMenu2X::deadLink) };
		voices.push_back(opt);
	}

	int sel = listbox(&voices);

	if(sel >= 0)
		wpaAdd(scriptOutput[sel]);
}

void GMenu2X::wpaAdd(string& SSID){
	//need to ask for the password type
	vector<MenuOption> voices;
	string strPassword;
	string strCommand;

	//trim leading and trailing spaces
	SSID.erase(remove_if(SSID.begin(), SSID.end(), ::isspace), SSID.end());

	{
	MenuOption opt = { tr["WPA/WPA2"], MakeDelegate(this, &GMenu2X::deadLink) };
	voices.push_back(opt);
	}{
	MenuOption opt = { tr["Hex WEP Key"], MakeDelegate(this, &GMenu2X::deadLink) };
	voices.push_back(opt);
	}{
	MenuOption opt = { tr["ASCII WEP Key"], MakeDelegate(this, &GMenu2X::deadLink) };
	voices.push_back(opt);
	}{
	MenuOption opt = { tr["none/open"], MakeDelegate(this, &GMenu2X::deadLink) };
	voices.push_back(opt);
	}

	int sel = listbox(&voices);

	if(sel < 0) return;
	//enter the password
	if(sel == 3)
		strCommand = "echo -e \"\nnetwork={\n\tssid=\\\"" + SSID + "\\\"\n\tkey_mgmt=NONE\n}\" >> /etc/wpa.conf";
	else{
		InputDialog id(this, input, ts, tr["Enter passphrase"],"", tr["Setup"], "skin:icons/wifi.png");
		if (id.exec() == false)
		return;

		strPassword = id.getInput();

		switch(sel) {
				case 0:
					strCommand = "echo -e \"\nnetwork={\n\tssid=\\\"" + SSID + "\\\"\n\tkey_mgmt=WPA-PSK\n\tpsk=\\\"" + strPassword.c_str() + "\\\"\n}\" >> /etc/wpa.conf";
					break;
				case 1:
				case 2:
					strCommand = "echo -e \"\nnetwork={\n\tssid=\\\"" + SSID + "\\\"\n\tkey_mgmt=NONE\n\twep_key0=\\\"" + strPassword.c_str() + "\\\"\n}\" >> /etc/wpa.conf";
					break;

				default:
					break;
			}
	}

	system(strCommand.c_str());

	MessageBox mb(this,tr["Connecting to wireless network..."],"skin:icons/wifi.png", MakeDelegate(this, &GMenu2X::wpaConnect));
	if(mb.exec() == 1){
		MessageBox mb(this,tr["Unable to connect with current settings."],"skin:icons/wifi.png");
		mb.exec();
	}

}

void GMenu2X::wpaConnect(MessageBox* pMsgBox, int& retVal){
	int ret = system( GMENU2X_SYSTEM_DIR "/scripts/wpa-connect");

	if(ret == 0){
		pMsgBox->setText("Connected...");
		sleep(3);

		nwifilevel = getWiFiLevel();
		bRedraw=true;
	}
	retVal = WEXITSTATUS(ret);
	return ;
}