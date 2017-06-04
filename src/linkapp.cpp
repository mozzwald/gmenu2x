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
#include <sys/types.h>
#include <sys/stat.h>
#if defined(TARGET_Z2)
#include <sys/ioctl.h>
#include <signal.h>
#endif
#include <unistd.h>

#include <fstream>
#include <sstream>

#include "linkapp.h"
#include "gmenu2x.h"
#include "menu.h"
#include "selector.h"
#include "textmanualdialog.h"
#include "debug.h"

using namespace std;

LinkApp::LinkApp(GMenu2X *gmenu2x_, InputManager &inputMgr_,
				 const char* linkfile)
	: Link(gmenu2x_)
	, inputMgr(inputMgr_)
{
	manual = "";
	file = linkfile;
	wrapper = false;
	dontleave = false;
	newterm = true; // default run app in new vt and keep gmenu2x running
	setClock(DEFAULT_CPU_CLK);
	setVolume(-1);
	//G
	setGamma(0);
	selectordir = "";
	selectorfilter = "";
	icon = iconPath = "";
	selectorbrowser = false;
	useRamTimings = false;
	workdir = "";

	string line;
	ifstream infile (linkfile, ios_base::in);
	while (getline(infile, line, '\n')) {
		line = trim(line);
		if (line=="") continue;
		if (line[0]=='#') continue;

		string::size_type position = line.find("=");
		string name = trim(line.substr(0,position));
		string value = trim(line.substr(position+1));
		if (name == "title") {
			title = value;
		} else if (name == "description") {
			description = value;
		} else if (name == "icon") {
			setIcon(value);
		} else if (name == "exec") {
			exec = value;
		} else if (name == "params") {
			params = value;
		} else if (name == "workdir") {
			workdir = value;
		} else if (name == "manual") {
			manual = value;
		} else if (name == "wrapper") {
			if (value=="true") wrapper = true;
		} else if (name == "dontleave") {
			if (value=="true") dontleave = true;
		} else if (name == "newterm") {
			if (value=="false") newterm = false;
		} else if (name == "clock") {
			setClock( atoi(value.c_str()) );
		//G
		} else if (name == "gamma") {
			setGamma( atoi(value.c_str()) );
		} else if (name == "volume") {
			setVolume( atoi(value.c_str()) );
		} else if (name == "selectordir") {
			setSelectorDir( value );
		} else if (name == "selectorbrowser") {
			if (value=="true") selectorbrowser = true;
		} else if (name == "useramtimings") {
			if (value=="true") useRamTimings = true;
		} else if (name == "selectorfilter") {
			setSelectorFilter( value );
		} else if (name == "selectorscreens") {
			setSelectorScreens( value );
		} else if (name == "selectoraliases") {
			setAliasFile( value );
		} else {
			WARNING("Unrecognized option: '%s'", name.c_str());
			break;
		}
	}
	infile.close();

	if (iconPath.empty()) searchIcon();

	edited = false;
}

const string &LinkApp::searchIcon() {
	string execicon = exec;
	string::size_type pos = exec.rfind(".");
	if (pos != string::npos) execicon = exec.substr(0,pos);
	execicon += ".png";
	string exectitle = execicon;
	pos = execicon.rfind("/");
	if (pos != string::npos)
		string exectitle = execicon.substr(pos+1,execicon.length());

	if (!gmenu2x->sc.getSkinFilePath("icons/"+exectitle).empty())
		iconPath = gmenu2x->sc.getSkinFilePath("icons/"+exectitle);
	else if (fileExists(execicon))
		iconPath = execicon;
	else
		iconPath = gmenu2x->sc.getSkinFilePath("icons/generic.png");

	return iconPath;
}

int LinkApp::clock() {
	return iclock;
}

const string &LinkApp::clockStr(int maxClock) {
	if (iclock>maxClock) setClock(maxClock);
	return sclock;
}

void LinkApp::setClock(int mhz) {
#ifdef TARGET_GP2X
	iclock = constrain(mhz,50,325);
#elif defined(TARGET_WIZ) || defined(TARGET_CAANOO)
	iclock = constrain(mhz,50,900);
#elif defined(TARGET_Z2)
	iclock = constrain(mhz,50,416);
#endif
	stringstream ss;
	sclock = "";
	ss << iclock << "MHz";
	ss >> sclock;

	edited = true;
}

int LinkApp::volume() {
	return ivolume;
}

const string &LinkApp::volumeStr() {
	return svolume;
}

void LinkApp::setVolume(int vol) {
	ivolume = constrain(vol,-1,100);
	stringstream ss;
	svolume = "";
	if (ivolume<0)
		ss << gmenu2x->confInt["globalVolume"];
	else
		ss << ivolume;
	ss >> svolume;

	edited = true;
}

//G
int LinkApp::gamma() {
	return igamma;
}

const string &LinkApp::gammaStr() {
	return sgamma;
}

void LinkApp::setGamma(int gamma) {
	igamma = constrain(gamma,0,100);
	stringstream ss;
	sgamma = "";
	ss << igamma;
	ss >> sgamma;

	edited = true;
}
// /G

bool LinkApp::targetExists() {
#if !defined(TARGET_GP2X) && !defined(TARGET_WIZ) && !defined(TARGET_CAANOO) && !defined(TARGET_Z2) 
	return true; //For displaying elements during testing on pc
#endif

	string target = exec;
	if (!exec.empty() && exec[0]!='/' && !workdir.empty())
		target = workdir + "/" + exec;

	return fileExists(target);
}

bool LinkApp::save() {
	if (!edited) return false;

	ofstream f(file.c_str());
	if (f.is_open()) {
		if (title!=""          ) f << "title="           << title           << endl;
		if (description!=""    ) f << "description="     << description     << endl;
		if (icon!=""           ) f << "icon="            << icon            << endl;
		if (exec!=""           ) f << "exec="            << exec            << endl;
		if (params!=""         ) f << "params="          << params          << endl;
		if (workdir!=""        ) f << "workdir="         << workdir         << endl;
		if (manual!=""         ) f << "manual="          << manual          << endl;
		if (iclock!=0          ) f << "clock="           << iclock          << endl;
		if (useRamTimings      ) f << "useramtimings=true"                  << endl;
		if (ivolume>0          ) f << "volume="          << ivolume         << endl;
		//G
		if (igamma!=0          ) f << "gamma="           << igamma          << endl;
		if (selectordir!=""    ) f << "selectordir="     << selectordir     << endl;
		if (selectorbrowser    ) f << "selectorbrowser=true"                << endl;
		if (selectorfilter!="" ) f << "selectorfilter="  << selectorfilter  << endl;
		if (selectorscreens!="") f << "selectorscreens=" << selectorscreens << endl;
		if (aliasfile!=""      ) f << "selectoraliases=" << aliasfile       << endl;
		if (wrapper            ) f << "wrapper=true"                        << endl;
		if (dontleave          ) f << "dontleave=true"                      << endl;
		if (newterm            ) f << "newterm=true"                      	<< endl;
		f.close();
#if defined(TARGET_Z2)
		sync();
#endif
		return true;
	} else
		ERROR("Error while opening the file '%s' for write.", file.c_str());
	return false;
}

void LinkApp::drawRun() {
	//Darkened background
	gmenu2x->s->box(0, 0, gmenu2x->resX, gmenu2x->resY, 0,0,0,150);

	string text = gmenu2x->tr.translate("Launching $1",getTitle().c_str(),NULL);
	int textW = gmenu2x->font->getTextWidth(text);
	int boxW = 62+textW;
	int halfBoxW = boxW/2;

	//outer box
	gmenu2x->s->box(gmenu2x->halfX-2-halfBoxW, gmenu2x->halfY-23, halfBoxW*2+5, 47, gmenu2x->skinConfColors[COLOR_MESSAGE_BOX_BG]);
	//inner rectangle
	gmenu2x->s->rectangle(gmenu2x->halfX-halfBoxW, gmenu2x->halfY-21, boxW, 42, gmenu2x->skinConfColors[COLOR_MESSAGE_BOX_BORDER]);

	int x = gmenu2x->halfX+10-halfBoxW;
	/*if (getIcon()!="")
		gmenu2x->sc[getIcon()]->blit(gmenu2x->s,x,104);
	else
		gmenu2x->sc["icons/generic.png"]->blit(gmenu2x->s,x,104);*/
	gmenu2x->sc[getIconPath()]->blit(gmenu2x->s,x,gmenu2x->halfY-16,32,32);
	gmenu2x->s->write( gmenu2x->font, text, x+42, gmenu2x->halfY+1, HAlignLeft, VAlignMiddle );
	gmenu2x->s->flip();
}

void LinkApp::run() {
	if (selectordir!="")
		selector();
	else
		launch();
}

void LinkApp::showManual() {
	if (manual=="" || !fileExists(manual)) return;

	// Png manuals
	string ext8 = manual.substr(manual.size()-8,8);
	if (ext8==".man.png" || ext8==".man.bmp" || ext8==".man.jpg" || manual.substr(manual.size()-9,9)==".man.jpeg") {
		Surface pngman(manual);
		Surface bg(gmenu2x->confStr["wallpaper"],false);
		stringstream ss;
		string pageStatus;

		bool close = false, repaint = true;
		int page=0, pagecount=pngman.raw->w/320;

		ss << pagecount;
		string spagecount;
		ss >> spagecount;

		while (!close) {
			if (repaint) {
				bg.blit(gmenu2x->s, 0, 0);
				pngman.blit(gmenu2x->s, -page*320, 0);

				gmenu2x->drawBottomBar();
				gmenu2x->drawButton(gmenu2x->s, "x", gmenu2x->tr["Exit"],
				gmenu2x->drawButton(gmenu2x->s, "right", gmenu2x->tr["Change page"],
				gmenu2x->drawButton(gmenu2x->s, "left", "", 5)-10));

				ss.clear();
				ss << page+1;
				ss >> pageStatus;
				pageStatus = gmenu2x->tr["Page"]+": "+pageStatus+"/"+spagecount;
				gmenu2x->s->write(gmenu2x->font, pageStatus, 310, 230, HAlignRight, VAlignMiddle);

				gmenu2x->s->flip();
				repaint = false;
			}

			inputMgr.update();
			if ( inputMgr[MANUAL] || inputMgr[CANCEL] || inputMgr[SETTINGS] ) close = true;
			if ( inputMgr[LEFT] && page>0 ) { page--; repaint=true; }
			if ( inputMgr[RIGHT] && page<pagecount-1 ) { page++; repaint=true; }
		}
		return;
	}

	// Txt manuals and readmes
	vector<string> txtman;

#ifdef ZIPIT_Z2
	string filename = manual;
	// Strip HTML tags for readme.
	if (manual.substr(manual.size()-5,5)==".html") {
	  string sedcmd = "sed -e 's/<[^>]*>/ /g' -e 's/<[^>]*>//g' ";
	  filename = "/tmp/gmenu2x.readme.txt";
	  sedcmd += manual + " > " +filename;
	  system(sedcmd.c_str());
	}
	extern int readtextfile(string filename, vector<string> &txtman);

	if (readtextfile(filename, txtman)) {
		// ZIPIT_Z2 // Save space -- Skip Manual file format.
		TextDialog td(gmenu2x, getTitle(), "ReadMe", getIconPath(), &txtman);
		td.exec();
	}
#else
	string line;
	ifstream infile(manual.c_str(), ios_base::in);
	if (infile.is_open()) {
		while (getline(infile, line, '\n'))
			txtman.push_back( strreplace(line, "\r", "") );
		infile.close();

		if (manual.substr(manual.size()-8,8)==".man.txt") {
			TextManualDialog tmd(gmenu2x, getTitle(), getIconPath(), &txtman);
			tmd.exec();
		} else {
			TextDialog td(gmenu2x, getTitle(), "ReadMe", getIconPath(), &txtman);
			td.exec();
		}
	}
#endif
}

void LinkApp::selector(int startSelection, const string &selectorDir) {
	//Run selector interface
	Selector sel(gmenu2x, this, selectorDir);
	int selection = sel.exec(startSelection);
	if (selection!=-1) {
		gmenu2x->writeTmp(selection, sel.getDir());
		launch(sel.getFile(), sel.getDir());
	}
}

void LinkApp::launch(const string &selectedFile, const string &selectedDir) {
	if (!newterm)
		drawRun();

	save();
#if !defined(TARGET_GP2X) && !defined(TARGET_WIZ) && !defined(TARGET_CAANOO) && !defined(TARGET_Z2) && !defined(TARGET_IZ2S)
	//delay for testing
	SDL_Delay(1000);
#endif

	//Set correct working directory
	string wd = getRealWorkdir();
	if (!wd.empty())
		chdir(wd.c_str());

#if defined(TARGET_Z2) || defined(TARGET_IZ2S) /* dontleave params bugfix */
	// Bug fix.  Gotta restore original params for multiple dontleave calls.
	string origParams = params;
#endif
	//selectedFile
	if (selectedFile!="") {
		string selectedFileExtension;
		string selectedFileName;
		string dir;
		string::size_type i = selectedFile.rfind(".");
		if (i != string::npos) {
			selectedFileExtension = selectedFile.substr(i,selectedFile.length());
			selectedFileName = selectedFile.substr(0,i);
		}

		if (selectedDir=="")
			dir = getSelectorDir();
		else
			dir = selectedDir;
		if (params=="") {
			params = cmdclean(dir+selectedFile);
		} else {
#if defined(TARGET_IZ2S) /* dontleave params bugfix */
			// Bug fix.  Gotta restore original params for multiple dontleave calls.
#else
			string origParams = params;
#endif
			params = strreplace(params,"[selFullPath]",cmdclean(dir+selectedFile));
			params = strreplace(params,"[selPath]",cmdclean(dir));
			params = strreplace(params,"[selFile]",cmdclean(selectedFileName));
			params = strreplace(params,"[selExt]",cmdclean(selectedFileExtension));
			if (params == origParams) params += " " + cmdclean(dir+selectedFile);
		}
	}

	if (useRamTimings)
		gmenu2x->applyRamTimings();
#ifdef TARGET_IZ2S /* ZIPIT_Z2_VOLUME */
	// Keep the current mixer volume settings and ignore the linkApp volume setting.
#else
	if (volume()>=0)
		gmenu2x->setVolume(volume());
#endif

	INFO("Executing '%s' (%s %s)", title.c_str(), exec.c_str(), params.c_str());

	//check if we have to quit
	string command = cmdclean(exec);

	// Check to see if permissions are desirable
	struct stat fstat;
	if( stat( command.c_str(), &fstat ) == 0 ) {
		struct stat newstat = fstat;
		if( S_IRUSR != ( fstat.st_mode & S_IRUSR ) )
			newstat.st_mode |= S_IRUSR;
		if( S_IXUSR != ( fstat.st_mode & S_IXUSR ) )
			newstat.st_mode |= S_IXUSR;
		if( fstat.st_mode != newstat.st_mode )
			chmod( command.c_str(), newstat.st_mode );
	} // else, well.. we are no worse off :)

#ifdef TARGET_Z2
	if (params!="") command += " " + params;
	if (gmenu2x->confInt["outputLogs"]) command += " &> " + cmdclean(gmenu2x->getExePath()) + "/log.txt";
	if (wrapper) command += "; sync & cd "+cmdclean(gmenu2x->getExePath())+"; exec ./gmenu2x";
	if (dontleave) {
		system(command.c_str());
	} else if(newterm){
		SDL_WM_IconifyWindow();
		command = GMENU2X_SYSTEM_DIR "/scripts/launch " + command;
		system(command.c_str());
	} else {
		if (gmenu2x->confInt["saveSelection"] && (gmenu2x->confInt["section"]!=gmenu2x->menu->selSectionIndex() || gmenu2x->confInt["link"]!=gmenu2x->menu->selLinkIndex()))
			gmenu2x->writeConfig();
		if (gmenu2x->fwType == "open2x" && gmenu2x->savedVolumeMode != gmenu2x->volumeMode)
			gmenu2x->writeConfigOpen2x();
		if (selectedFile=="")
			gmenu2x->writeTmp();
#ifdef TARGET_Z2
		remove("/tmp/vt/gmenu2x");
#endif
		gmenu2x->quit();
		if (clock()!=gmenu2x->confInt["menuClock"])
			gmenu2x->setClock(clock());
		if (gamma()!=0 && gamma()!=gmenu2x->confInt["gamma"])
			gmenu2x->setGamma(gamma());

		/* Make the terminal we're connected to (via stdin/stdout) our
		   controlling terminal again.  Else many console programs are
		   not going to work correctly.  Actually this would not be
		   necessary, if SDL correctly restored terminal state after
		   SDL_Quit(). */
		pid_t pid = setsid();
		if (pid == (pid_t)-1) {
			WARNING("Failed to create new process group\n");
		}
		ioctl(1, TIOCSCTTY, STDOUT_FILENO);

		int pgid = tcgetpgrp(STDOUT_FILENO);
		signal(SIGTTOU, SIG_IGN);
		tcsetpgrp(STDOUT_FILENO, pgid);
		/* End controlling terminal fix */
#ifdef TARGET_Z2
		gmenu2x->writePID();
#endif
		execlp("/bin/sh","/bin/sh","-c",command.c_str(),NULL);
		//if execution continues then something went wrong and as we already called SDL_Quit we cannot continue
		//try relaunching gmenu2x
		chdir(gmenu2x->getExePath().c_str());
		execlp("./gmenu2x", "./gmenu2x", NULL);
	}
#endif
#ifdef TARGET_IZ2S
	extern char *progpath;
	string filepath = progpath; //gmenu2x->getExePath();

	// Trap INT and TERM signals or they will kill this wrapper before the return to gmenu. 
	command = "trap - INT TERM; " + cmdclean(exec);
	if (params!="") command += " " + params;
	//if (gmenu2x->confInt["outputLogs"]) command += " &> " + cmdclean(gmenu2x->getExePath()) + "/log.txt";
	if (gmenu2x->confInt["outputLogs"]) command += " &> " + filepath + "/log.txt";
	// Force uclibc to end the suspended parent process with a killall.
	//if (wrapper) command += "; sync & cd "+cmdclean(gmenu2x->getExePath())+"; killall -9 gmenu2x; exec ./gmenu2x";
	if (wrapper) command += "; sync & cd "+ cmdclean(filepath) +"; killall -9 gmenu2x; exec ./gmenu2x";
	if (dontleave) {
	        // Bug fix.  Gotta restore original params for multiple dontleave calls.
		params = origParams;
		system(command.c_str());
	} else {
		if (gmenu2x->confInt["saveSelection"] && (gmenu2x->confInt["section"]!=gmenu2x->menu->selSectionIndex() || gmenu2x->confInt["link"]!=gmenu2x->menu->selLinkIndex()))
			gmenu2x->writeConfig();
		if (gmenu2x->fwType == "open2x" && gmenu2x->savedVolumeMode != gmenu2x->volumeMode)
			gmenu2x->writeConfigOpen2x();
		if (selectedFile=="")
			gmenu2x->writeTmp();
		gmenu2x->quit();
		if (clock()!=gmenu2x->confInt["menuClock"])
			gmenu2x->setClock(clock());
		if (gamma()!=0 && gamma()!=gmenu2x->confInt["gamma"])
			gmenu2x->setGamma(gamma());

		// ---- Got this nice fix from the nanonote git sources ----
		/* Make the terminal we're connected to (via stdin/stdout) our
      		   contolling terminal again.  Else many console programs are
      		   not going to work correctly.  Actually this would not be
      		   necessary, if SDL correctly restored terminal state after
      		   SDL_Quit(). */
		int pid = setsid();
		//ioctl(1, TIOCSCTTY, STDOUT_FILENO);
		ioctl(0, TIOCSCTTY, STDOUT_FILENO);

		int pgid = tcgetpgrp(STDOUT_FILENO);
		signal(SIGTTOU, SIG_IGN);
		tcsetpgrp(STDOUT_FILENO, pgid);
		// ---- Got this nice fix from the nanonote git sources ----

		// ---- Try to avoid NODELAY warnings and bad nonblocking behavior ----
		fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~O_NONBLOCK);

		execlp("/bin/sh","/bin/sh","-c",command.c_str(),NULL);

		//if execution continues then something went wrong and as we already called SDL_Quit we cannot continue
		//try relaunching gmenu2x
		//chdir(gmenu2x->getExePath().c_str());
		chdir(filepath.c_str());
		execlp("./gmenu2x", "./gmenu2x", NULL);
	}
#endif
#if 0
	if (params!="") command += " " + params;
	if (gmenu2x->confInt["outputLogs"]) command += " &> " + cmdclean(gmenu2x->getExePath()) + "/log.txt";
	// dontleave does not call gmenu2x->quit so SDL still owns the screen.  Console progs will be offscreen.
	//if (wrapper && !dontleave) command += "; sync & cd "+cmdclean(gmenu2x->getExePath())+"; exec ./gmenu2x";
	// Dont use exec on the gmenu2x call for ZIPIT_Z2 running IZ2S
	if (wrapper && !dontleave) command += "; sync; cd "+cmdclean(gmenu2x->getExePath())+"; ./gmenu2x";
	if (dontleave) {
		system(command.c_str());
	} else {
		if (gmenu2x->confInt["saveSelection"] && (gmenu2x->confInt["section"]!=gmenu2x->menu->selSectionIndex() || gmenu2x->confInt["link"]!=gmenu2x->menu->selLinkIndex()))
			gmenu2x->writeConfig();
		if (gmenu2x->fwType == "open2x" && gmenu2x->savedVolumeMode != gmenu2x->volumeMode)
			gmenu2x->writeConfigOpen2x();
		if (selectedFile=="")
			gmenu2x->writeTmp();
		gmenu2x->quit();
		if (clock()!=gmenu2x->confInt["menuClock"])
			gmenu2x->setClock(clock());
		if (gamma()!=0 && gamma()!=gmenu2x->confInt["gamma"])
			gmenu2x->setGamma(gamma());

		// Apparently the uclibc execlp() fn in IZ2S merely suspends the parent process
		// until after the child process exits, then it lets the parent die.
		// This is trouble for wrapper because the child execs a new gmenu2x instead of exiting.
		if (wrapper)
		{
		  // Can I add a fork to allow me kill off the parent, or is that broken too?
		  //		  int pid = fork(); // May create zombies if I dont wait on the child.  Oh well.
		  //		  if (pid == 0)

			char wrapfile[] = "/tmp/gmenu2x.XXXXXX";
			int fd = mkstemp(wrapfile);
			fchmod(fd, 0744);
			write(fd, "#!/bin/sh\n", strlen("#!/bin/sh\n"));
			write(fd, command.c_str(), strlen(command.c_str()));
			write(fd, "\n", strlen("\n"));
			fsync(fd);
			close(fd);
			sleep(1);
			// execlp("/bin/sh","/bin/sh","-c",wrapfile,NULL); // 
			//execlp(wrapfile,wrapfile,NULL); // Weird, busybox reports "applet not found".
			execlp(wrapfile,wrapfile,NULL); 
		}
		else
			execlp("/bin/sh","/bin/sh","-c",command.c_str(),NULL);

		// If execution continues then we are the parent, or something went wrong.
		// If not parent, we already called SDL_Quit so we cannot continue.  Just quit.
		exit(0);
	}
#endif

#ifdef TARGET_IZ2S
	chdir(filepath.c_str());
#else
	chdir(gmenu2x->getExePath().c_str());
#endif
}

const string &LinkApp::getExec() {
	return exec;
}

void LinkApp::setExec(const string &exec) {
	this->exec = exec;
	edited = true;
}

const string &LinkApp::getParams() {
	return params;
}

void LinkApp::setParams(const string &params) {
	this->params = params;
	edited = true;
}

const string &LinkApp::getWorkdir() {
	return workdir;
}

const string LinkApp::getRealWorkdir() {
	string wd = workdir;
	if (wd.empty()) {
		if (exec[0]!='/') {
			wd = gmenu2x->getExePath();
		} else {
			string::size_type pos = exec.rfind("/");
			if (pos != string::npos)
				wd = exec.substr(0,pos);
		}
	}
	return wd;
}

void LinkApp::setWorkdir(const string &workdir) {
	this->workdir = workdir;
	edited = true;
}

const string &LinkApp::getManual() {
	return manual;
}

void LinkApp::setManual(const string &manual) {
	this->manual = manual;
	edited = true;
}

const string &LinkApp::getSelectorDir() {
	return selectordir;
}

void LinkApp::setSelectorDir(const string &selectordir) {
	this->selectordir = selectordir;
	if (this->selectordir!="" && this->selectordir[this->selectordir.length()-1]!='/') this->selectordir += "/";
	edited = true;
}

bool LinkApp::getSelectorBrowser() {
	return selectorbrowser;
}

void LinkApp::setSelectorBrowser(bool value) {
	selectorbrowser = value;
	edited = true;
}

bool LinkApp::getUseRamTimings() {
	return useRamTimings;
}

void LinkApp::setUseRamTimings(bool value) {
	useRamTimings = value;
	edited = true;
}

const string &LinkApp::getSelectorFilter() {
	return selectorfilter;
}

void LinkApp::setSelectorFilter(const string &selectorfilter) {
	this->selectorfilter = selectorfilter;
	edited = true;
}

const string &LinkApp::getSelectorScreens() {
	return selectorscreens;
}

void LinkApp::setSelectorScreens(const string &selectorscreens) {
	this->selectorscreens = selectorscreens;
	edited = true;
}

const string &LinkApp::getAliasFile() {
	return aliasfile;
}

void LinkApp::setAliasFile(const string &aliasfile) {
	if (fileExists(aliasfile)) {
		this->aliasfile = aliasfile;
		edited = true;
	}
}

void LinkApp::renameFile(const string &name) {
	file = name;
}
