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

//for browsing the filesystem
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <strings.h>
#include <math.h>
#include <unistd.h>

#include <SDL.h>

#include "utilities.h"
#include "debug.h"

using namespace std;

bool case_less::operator()(const string &left, const string &right) const {
	return strcasecmp(left.c_str(), right.c_str()) < 0;
}

// General tool to strip spaces from both ends:
string trim(const string& s) {
  if(s.length() == 0)
    return s;
  int b = s.find_first_not_of(" \t\r");
  int e = s.find_last_not_of(" \t\r");
  if(b == -1) // No non-spaces
    return "";
  return string(s, b, e - b + 1);
}

void string_copy(const string &s, char **cs) {
	*cs = (char*)malloc(s.length());
	strcpy(*cs, s.c_str());
}

char * string_copy(const string &s) {
	char *cs = NULL;
	string_copy(s, &cs);
	return cs;
}

bool fileExists(const string &file) {
	fstream fin;
	fin.open(file.c_str() ,ios::in);
	bool exists = fin.is_open();
	fin.close();

	return exists;
}

bool rmtree(string path) {
	DIR *dirp;
	struct stat st;
	struct dirent *dptr;
	string filepath;

	DEBUG("RMTREE: '%s'", path.c_str());

	if ((dirp = opendir(path.c_str())) == NULL) return false;
	if (path[path.length()-1]!='/') path += "/";

	while ((dptr = readdir(dirp))) {
		filepath = dptr->d_name;
		if (filepath=="." || filepath=="..") continue;
		filepath = path+filepath;
		int statRet = stat(filepath.c_str(), &st);
		if (statRet == -1) continue;
		if (S_ISDIR(st.st_mode)) {
			if (!rmtree(filepath)) return false;
		} else {
			if (unlink(filepath.c_str())!=0) return false;
		}
	}

	closedir(dirp);
	return rmdir(path.c_str())==0;
}

int max (int a, int b) {
	return a>b ? a : b;
}
int min (int a, int b) {
	return a<b ? a : b;
}
int constrain (int x, int imin, int imax) {
	return min( imax, max(imin,x) );
}

//Configuration parsing utilities
int evalIntConf (int val, int def, int imin, int imax) {
	if (val==0 && (val<imin || val>imax))
		return def;
	val = constrain(val, imin, imax);
	return val;
}
int evalIntConf (int *val, int def, int imin, int imax) {
	*val = evalIntConf(*val, def, imin, imax);
	return *val;
}

const string &evalStrConf (const string &val, const string &def) {
	return val.empty() ? def : val;
}
const string &evalStrConf (string *val, const string &def) {
	*val = evalStrConf(*val, def);
	return *val;
}

float max (float a, float b) {
	return a>b ? a : b;
}
float min (float a, float b) {
	return a<b ? a : b;
}
float constrain (float x, float imin, float imax) {
	return min( imax, max(imin,x) );
}

bool split (vector<string> &vec, const string &str, const string &delim, bool destructive) {
	vec.clear();

	if (delim.empty()) {
		vec.push_back(str);
		return false;
	}

	std::string::size_type i = 0;
	std::string::size_type j = 0;

	while(1) {
		j = str.find(delim,i);
		if (j==std::string::npos) {
			vec.push_back(str.substr(i));
			break;
		}

		if (!destructive)
			j += delim.size();

		vec.push_back(str.substr(i,j-i));

		if (destructive)
			i = j + delim.size();

		if (i==str.size()) {
			vec.push_back(std::string());
			break;
		}
	}

	return true;
}

string strreplace (string orig, const string &search, const string &replace) {
	string::size_type pos = orig.find( search, 0 );
	while (pos != string::npos) {
		orig.replace(pos,search.length(),replace);
		pos = orig.find( search, pos+replace.length() );
	}
	return orig;
}

string cmdclean (string cmdline) {
#if defined(TARGET_Z2) // For now do not clean * (wildcard) from cmdline chars.
	string spchars = "\\`$();|{}&'\"?<>[]!^~-#\n\r ";
#else
	string spchars = "\\`$();|{}&'\"*?<>[]!^~-#\n\r ";
#endif
	for (uint i=0; i<spchars.length(); i++) {
		string curchar = spchars.substr(i,1);
		cmdline = strreplace(cmdline, curchar, "\\"+curchar);
	}
	return cmdline;
}

int intTransition(int from, int to, long tickStart, long duration, long tickNow) {
	if (tickNow<0) tickNow = SDL_GetTicks();
	float elapsed = (float)(tickNow-tickStart)/duration;
	//                    elapsed                 increments
	return constrain(round(elapsed*(to-from)),from,to);
}

string exec(const char* cmd) {
	FILE* pipe = popen(cmd, "r");
	if (!pipe) return "";
	char buffer[128];
	string result = "";
	while (!feof(pipe)) {
		if(fgets(buffer, 128, pipe) != NULL)
			result += buffer;
	}
	pclose(pipe);
	return result;
}

#if 1
#include <stdlib.h>
/*
  Check if the given unsigned char * is a valid utf-8 sequence.

  Return value :
  If the string is valid utf-8, 0 is returned.
  Else the position, starting from 1, is returned.

  Valid utf-8 sequences look like this :
  0xxxxxxx
  110xxxxx 10xxxxxx
  1110xxxx 10xxxxxx 10xxxxxx
  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
*/
int utf8_invalid(unsigned char *str, size_t len)
{
    size_t i = 0;
    size_t continuation_bytes = 0;

    while (i < len)
    {
        if (str[i] <= 0x7F)
            continuation_bytes = 0;
        else if (str[i] >= 0xC0 /*11000000*/ && str[i] <= 0xDF /*11011111*/)
            continuation_bytes = 1;
        else if (str[i] >= 0xE0 /*11100000*/ && str[i] <= 0xEF /*11101111*/)
            continuation_bytes = 2;
        else if (str[i] >= 0xF0 /*11110000*/ && str[i] <= 0xF4 /* Cause of RFC 3629 */)
            continuation_bytes = 3;
        else
            return i + 1;
        i += 1;
        while (i < len && continuation_bytes > 0
               && str[i] >= 0x80
               && str[i] <= 0xBF)
        {
            i += 1;
            continuation_bytes -= 1;
        }
        if (continuation_bytes != 0)
            return i + 1;
    }
    return 0;
}

int readtextfile(string filename, vector<string> &txtman)
{
	string line;
	ifstream infile(filename.c_str(), ios_base::in);

	if (infile.is_open()) {
		while (getline(infile, line, '\n'))
		{
			// Convert ISO 8851-15 to utf-8 (8 chars differ from IS0 8851-1)
			extern int utf8_invalid(unsigned char *str, size_t len);
			int latin = utf8_invalid((unsigned char *)line.c_str(), line.length());
			// check for invalid utf-8
			string utf8line = "";
			unsigned char *s = (unsigned char *)line.c_str();
			unsigned char ch;
			for (uint j=0; j<line.length(); j++) {
			  ch = s[j];
			  if (ch == '\t') // Tab expansion
			    for (uint k=0; k<(8-(k%8)); k++)
			      utf8line.push_back(' ');
			  else if (ch < 0x80) // ASCII
			    utf8line.push_back(ch);
			  else if (latin) switch (ch) 
			  {
			  case 0xa4: // Euro symbol in ISO 8851-15
			    utf8line.append("\xe2\x82\xac");
			    break;
			  case 0xa6: // S symbol in ISO 8851-15
			    utf8line.append("\xc5\xa0");
			    break;
			  case 0xa8: // s symbol in ISO 8851-15
			    utf8line.append("\xc5\xa1");
			    break;
			  case 0xb4: // Z symbol in ISO 8851-15
			    utf8line.append("\xc5\xbd");
			    break;
			  case 0xb8: // z symbol in ISO 8851-15
			    utf8line.append("\xc5\xbe");
			    break;
			  case 0xbc: // OE symbol in ISO 8851-15
			    utf8line.append("\xc5\x92");
			    break;
			  case 0xbd: // oe symbol in ISO 8851-15
			    utf8line.append("\xc5\x93");
			    break;
			  case 0xbe: // Y symbol in ISO 8851-15
			    utf8line.append("\xe2\xb8");
			    break;
			  default:
			    utf8line.push_back(0xc2+(ch>0xbf));
			    utf8line.push_back(0x80+(ch&0x3f));
			    break;
			  }
			  else // not latin15
			    utf8line.push_back(ch);
			}
			txtman.push_back( strreplace(utf8line, "\r", "") );
		}
		infile.close();
		return 1;
	}
	else return 0;
}


static int acute_map[] = {
0xC1,'B','C','D',0xC9,'F','G','H',0xCD,'J','K','L','M','N',0xD3,'P','Q','R','S','T',0xDA,'V','W','X',0xDD,'Z',
'\'',0,0,0,0,0,
0xE1,'b','c','d',0xE9,'f','g','h',0xED,'j','k','l','m','n',0xF3,'p','q','r','s','t',0xFA,'v','w','x',0xFD,'z'};
static int grave_map[] = {
0xC0,'B','C','D',0xC8,'F','G','H',0xCC,'J','K','L','M','N',0xD2,'P','Q','R','S','T',0xD9,'V','W','X','Y','Z',
'`',0,0,0,0,0,
0xE0,'b','c','d',0xE8,'f','g','h',0xEC,'j','k','l','m','n',0xF2,'p','q','r','s','t',0xF9,'v','w','x','y','z'};
static int circumflex_map[] = {
0xC2,'B','C','D',0xCA,'F','G','H',0xCE,'J','K','L','M','N',0xD4,'P','Q','R','S','T',0xDB,'V','W','X','Y','Z',
'^',0,0,0,0,0,
0xE2,'b','c','d',0xEA,'f','g','h',0xEE,'j','k','l','m','n',0xF4,'p','q','r','s','t',0xFB,'v','w','x','y','z'};
static int diaeresis_map[] = {
0xC4,'B','C','D',0xCB,'F','G','H',0xCF,'J','K','L','M','N',0xD6,'P','Q','R','S','T',0xDC,'V','W','X','Y','Z',
'\"',0,0,0,0,0,
0xE4,'b','c','d',0xEB,'f','g','h',0xEF,'j','k','l','m','n',0xF6,'p','q','r','s','t',0xFC,'v','w','x',0xFF,'z'};
static int cedilla_map[] = {
'A','B',0xC7,'D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
',',0,0,0,0,0,
'a','b',0xE7,'d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z'};
static int tilde_map[] = {
0xC3,'B','C','D','E','F','G','H','I','J','K','L','M',0xD1,0xD5,'P','Q','R','S','T','U','V','W','X','Y','Z',
'~',0,0,0,0,0,
0xE3,'b','c','d','e','f','g','h','i','j','k','l','m',0xF1,0xF5,'p','q','r','s','t','u','v','w','x','y','z'};

int processTextKey(SDL_Event event, string &input)
{
  static int dead_key = 0;

  int key = event.key.keysym.sym;
  int fl = event.key.keysym.mod;

  if (((key >= 'a') && (key <= 'z')) || (key == ' '))
  {
    if (fl & KMOD_SHIFT) 
      key = toupper(key);
    
    //if (!(fl & KMOD_CTRL) && (event.key.keysym.unicode == 0)) // Dead key
    if (event.key.keysym.unicode == 0) // Dead key
    {
      switch (key) {
      case 'A':
      case 'G':
      case 'D':
      case 'F':
      case 'T':
      case 'C':
	dead_key = key;
      default:
	return 0;
      case 'V':
	dead_key = 'C'; // (doppleganger for c)
	return 0;
      }
      //if (dead_key) fprintf(fp, "  dead=%c\n", dead_key);
      //else fprintf(fp, "  undead=0\n");
      return 0;	/* skip COMPOSE (dead) keys */
    }
    else if (dead_key)
    {
      key = event.key.keysym.unicode;
      if((key == 'V') && ((fl & KMOD_ALT) && (fl & KMOD_SHIFT)))
      {
	//fprintf(fp, "  decomposing %c+<%c>", dead_key, key);
	return 0; // (doppleganger for c)
      }
      //fprintf(fp, "  composing %c+<%c>", dead_key, key);
      if (key == ' ')
	key = 'Z'+1;
      if (dead_key == 'G') // (event.key.keysym.unicode == 0x300) // grave 
	key = grave_map[key - 'A'];
      else if (dead_key == 'A') // (event.key.keysym.unicode == 0x301) // acute 
	key = acute_map[key - 'A'];
      else if (dead_key == 'C') // (event.key.keysym.unicode == 0x302) // circum 
	key = circumflex_map[key - 'A'];
      else if (dead_key == 'T') // (event.key.keysym.unicode == 0x303) // tilde 
	key = tilde_map[key - 'A'];
      else if (dead_key == 'D') // (event.key.keysym.unicode == 0x308) // dia 
	key = diaeresis_map[key - 'A'];
      else if (dead_key == 'F') // (event.key.keysym.unicode == 0x327) // cedilla 
	key = cedilla_map[key - 'A'];
      dead_key = 0;
      //fprintf(fp, " = +<%c>\n", key);
    }
    else if((key == 'P') && ((fl & KMOD_ALT) && (fl & KMOD_SHIFT)))
      return 0; // (doppleganger for o)
    else if((key == 'Q') && ((fl & KMOD_ALT) && (fl & KMOD_SHIFT)))
      key = 0xBF; // Happier place for upside down question
    else if((key == 'W') && ((fl & KMOD_ALT) && (fl & KMOD_SHIFT)))
      return 0; // (doppleganger for e)
    else if((key == 'V') && ((fl & KMOD_ALT) && (fl & KMOD_SHIFT)))
    {
      dead_key = 'C';  // Happier place for circumflex
      //fprintf(fp, "  fakeDEAD=%c\n", dead_key);
      return 0;
    }
    else if (event.key.keysym.unicode == 0xa4) // Convert "currency" to "euro".
      key = 0x20ac;
    else
    {
      dead_key = 0;
      //if (fl & KMOD_CTRL)
      //  { /* Skip unicode translation for ctrl keys. */ } 
      key = event.key.keysym.unicode;
    }
    event.key.keysym.unicode = key;
  }

  if (event.key.keysym.unicode != 0)
  {
    if ((event.key.keysym.unicode & 0xff80) == 0) // ASCII
      input += (char)(event.key.keysym.unicode & 0x7F);
    else if (event.key.keysym.unicode < 0x800)
    {
      // UCS-2 Unicode.  Convert to UTF-8
      input += (char)(0xC0 | event.key.keysym.unicode>>6);
      input += (char)(0x80 | event.key.keysym.unicode & 0x3F);
    }
    else
    {
      input += (char)(0xE0 | event.key.keysym.unicode>>12);
      input += (char)(0x80 | event.key.keysym.unicode>>6 & 0x3F);
      input += (char)(0x80 | event.key.keysym.unicode & 0x3F);
    }
    return 1;
  }

  return 0;
}

#endif
			
