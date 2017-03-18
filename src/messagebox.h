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

#ifndef MESSAGEBOX_H_
#define MESSAGEBOX_H_

#define MB_BTN_B 0
#define MB_BTN_X 1
#define MB_BTN_START 2
#define MB_BTN_SELECT 3

#include <string>
#include "gmenu2x.h"
#include "FastDelegate.h"

using std::string;
using std::vector;

typedef fastdelegate::FastDelegate2<MessageBox*, int&, void> BlockingAction;

class MessageBox {
private:
	string text, icon;
	GMenu2X *gmenu2x;
	vector<string> buttons;
	vector<string> buttonLabels;
	vector<SDL_Rect> buttonPositions;
	BlockingAction action;
	void Draw();

public:
	MessageBox(GMenu2X *gmenu2x, const string &text, const string &icon="", BlockingAction act=0);
	void setButton(int action, const string &btn);
	void setText(const std::string &str);
	int exec();
};

#endif /*MESSAGEBOX_H_*/
