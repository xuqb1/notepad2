// Scintilla source code edit control
/** @file LexVerilog.cxx
 ** Lexer for Verilog.
 ** Written by Avi Yegudin, based on C++ lexer by Neil Hodgson
 **/
// Copyright 1998-2002 by Neil Hodgson <neilh@scintilla.org>
// The License.txt file describes the conditions under which this software may be distributed.

#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

using namespace Scintilla;

static bool IsVWordChar(const int ch) {
	return (ch < 0x80) && (isalnum(ch) || ch == '.' || ch == '_' || ch == '\''|| ch == '$');
}

static bool IsVWordStart(const int ch) {
	return (ch < 0x80) && (isalnum(ch) || ch == '_' || ch == '$');
}

/*static const char * const verilogWordLists[] = {
	"Primary keywords and identifiers",
	"Secondary keywords and identifiers",
	"System Tasks",
	"User defined tasks and identifiers",
	"Unused",
	0,
};*/

static void ColouriseVerilogDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	const WordList &keywords = *keywordLists[0];
	const WordList &keywords2 = *keywordLists[1];
	const WordList &keywords3 = *keywordLists[2];
	const WordList &keywords4 = *keywordLists[3];

	// Do not leak onto next line
	if (initStyle == SCE_V_STRINGEOL)
		initStyle = SCE_V_DEFAULT;

	StyleContext sc(startPos, length, initStyle, styler);

	for (; sc.More(); sc.Forward()) {

		if (sc.atLineStart && (sc.state == SCE_V_STRING)) {
			// Prevent SCE_V_STRINGEOL from leaking back to previous line
			sc.SetState(SCE_V_STRING);
		}
		// Handle line continuation generically.
		if (sc.ch == '\\') {
			if (sc.chNext == '\n' || sc.chNext == '\r') {
				sc.Forward();
				if (sc.ch == '\r' && sc.chNext == '\n') {
					sc.Forward();
				}
				continue;
			}
		}
		// Determine if the current state should terminate.
		if (sc.state == SCE_V_OPERATOR) {
			sc.SetState(SCE_V_DEFAULT);
		} else if (sc.state == SCE_V_NUMBER) {
			if (!IsVWordChar(sc.ch)) {
				sc.SetState(SCE_V_DEFAULT);
			}
		} else if (sc.state == SCE_V_IDENTIFIER) {
_label_identifier:
			if (!IsVWordChar(sc.ch) || (sc.ch == '.')) {
				char s[128];
				sc.GetCurrent(s, sizeof(s));
				if (keywords.InList(s)) {
					sc.ChangeState(SCE_V_WORD);
				} else if (s[0] == '$' && keywords2.InList(&s[1])) { // $ System Tasks and Functions
					sc.ChangeState(SCE_V_WORD2);
				} else if (keywords3.InList(s)) {
					sc.ChangeState(SCE_V_WORD3);
				} else if (keywords4.InList(s)) {
					sc.ChangeState(SCE_V_USER);
				}
				sc.SetState(SCE_V_DEFAULT);
			}
		} else if (sc.state == SCE_V_PREPROCESSOR) {
			if (!IsVWordChar(sc.ch)) {
				sc.SetState(SCE_V_DEFAULT);
			}
		} else if (sc.state == SCE_V_COMMENT) {
			if (sc.Match('*', '/')) {
				sc.Forward();
				sc.ForwardSetState(SCE_V_DEFAULT);
			}
		} else if (sc.state == SCE_V_COMMENTLINE || sc.state == SCE_V_COMMENTLINEBANG) {
			if (sc.atLineStart) {
				sc.SetState(SCE_V_DEFAULT);
			}
		} else if (sc.state == SCE_V_STRING) {
			if (sc.ch == '\\') {
				if (sc.chNext == '\"' || sc.chNext == '\'' || sc.chNext == '\\') {
					sc.Forward();
				}
			} else if (sc.ch == '\"') {
				sc.ForwardSetState(SCE_V_DEFAULT);
			} else if (sc.atLineEnd) {
				sc.ChangeState(SCE_V_STRINGEOL);
				sc.ForwardSetState(SCE_V_DEFAULT);
			}
		}
		// Determine if a new state should be entered.
		if (sc.state == SCE_V_DEFAULT) {
			if (IsADigit(sc.ch) || (sc.ch == '\'') || (sc.ch == '.' && IsADigit(sc.chNext))) {
				sc.SetState(SCE_V_NUMBER);
			} else if (IsVWordStart(sc.ch)) {
				sc.SetState(SCE_V_IDENTIFIER);
			} else if (sc.Match('/', '*')) {
				sc.SetState(SCE_V_COMMENT);
				sc.Forward();	// Eat the * so it isn't used for the end of the comment
			} else if (sc.Match('/', '/')) {
				if (sc.Match("//!"))	// Nice to have a different comment style
					sc.SetState(SCE_V_COMMENTLINEBANG);
				else
					sc.SetState(SCE_V_COMMENTLINE);
			} else if (sc.ch == '\"') {
				sc.SetState(SCE_V_STRING);
			} else if (sc.ch == '`') {
				sc.SetState(SCE_V_PREPROCESSOR);
				// Skip whitespace between ` and preprocessor word
				do {
					sc.Forward();
				} while ((sc.ch == ' ' || sc.ch == '\t') && sc.More());
				if (sc.atLineEnd) {
					sc.SetState(SCE_V_DEFAULT);
				}
			} else if (isoperator(static_cast<char>(sc.ch)) || sc.ch == '@' || sc.ch == '#') {
				sc.SetState(SCE_V_OPERATOR);
			}
		}
	}

	if (sc.state == SCE_V_IDENTIFIER)
		goto _label_identifier;

	sc.Complete();
}

static bool IsStreamCommentStyle(int style) {
	return style == SCE_V_COMMENT;
}
#define IsCommentLine(line) IsLexCommentLine(line, styler, MultiStyle(SCE_V_COMMENTLINE, SCE_V_COMMENTLINEBANG))

static void FoldVerilogDoc(Sci_PositionU startPos, Sci_Position length, int initStyle, LexerWordList, Accessor &styler) {
	if (styler.GetPropertyInt("fold") == 0)
		return;
	const bool foldComment = styler.GetPropertyInt("fold.comment", 1) != 0;
	const bool foldPreprocessor = styler.GetPropertyInt("fold.preprocessor", 1) != 0;
	const bool foldCompact = styler.GetPropertyInt("fold.compact", 0) != 0;
	const bool foldAtElse = styler.GetPropertyInt("fold.at.else", 0) != 0;

	Sci_PositionU endPos = startPos + length;
	int visibleChars = 0;
	Sci_Position lineCurrent = styler.GetLine(startPos);
	int levelCurrent = SC_FOLDLEVELBASE;
	if (lineCurrent > 0)
		levelCurrent = styler.LevelAt(lineCurrent-1) >> 16;
	int levelMinCurrent = levelCurrent;
	int levelNext = levelCurrent;

	char chNext = styler[startPos];
	int style = initStyle;
	int styleNext = styler.StyleAt(startPos);

	for (Sci_PositionU i = startPos; i < endPos; i++) {
		char ch = chNext;
		chNext = styler.SafeGetCharAt(i + 1);
		int stylePrev = style;
		style = styleNext;
		styleNext = styler.StyleAt(i + 1);
		bool atEOL = (ch == '\r' && chNext != '\n') || (ch == '\n');

		if (foldComment && IsStreamCommentStyle(style)) {
			if (!IsStreamCommentStyle(stylePrev)) {
				levelNext++;
			} else if (!IsStreamCommentStyle(styleNext) && !atEOL) {
				levelNext--;
			}
		}
		if (foldComment && atEOL && IsCommentLine(lineCurrent)) {
			if (!IsCommentLine(lineCurrent - 1) && IsCommentLine(lineCurrent + 1))
				levelNext++;
			else if (IsCommentLine(lineCurrent - 1) && !IsCommentLine(lineCurrent + 1))
				levelNext--;
		}
		if (foldPreprocessor && ch == '`' && (style == SCE_V_PREPROCESSOR)) {
			Sci_Position pos = LexSkipSpaceTab(i+1, endPos, styler);
			if (styler.Match(pos, "if")) {
				levelNext++;
			} else if (styler.Match(pos, "end")) {
				levelNext--;
			}
		}
		if (style == SCE_V_OPERATOR) {
			if (ch == '{' || ch == '(') {
				levelNext++;
			} else if (ch == '}' || ch == ')') {
				levelNext--;
			}
		}
		if (style == SCE_V_WORD && stylePrev != SCE_V_WORD) {
			Sci_PositionU j = i;
			if (styler.Match(j, "case") ||
				styler.Match(j, "class") || styler.Match(j, "function") || styler.Match(j, "generate") ||
				styler.Match(j, "covergroup") || styler.Match(j, "package") ||styler.Match(j, "primitive") ||
				styler.Match(j, "program") || styler.Match(j, "sequence") || styler.Match(j, "specify") ||
				styler.Match(j, "table") || styler.Match(j, "task") || styler.Match(j, "fork") ||
				styler.Match(j, "module") || styler.Match(j, "begin")) {
				levelNext++;
			} else if (styler.Match(j, "end") || styler.Match(j, "join")) {
				levelNext--;
			}
		}
		if (!isspacechar(ch))
			visibleChars++;
		if (atEOL || (i == endPos-1)) {
			int levelUse = levelCurrent;
			if (foldAtElse) {
				levelUse = levelMinCurrent;
			}
			int lev = levelUse | levelNext << 16;
			if (visibleChars == 0 && foldCompact)
				lev |= SC_FOLDLEVELWHITEFLAG;
			if (levelUse < levelNext)
				lev |= SC_FOLDLEVELHEADERFLAG;
			if (lev != styler.LevelAt(lineCurrent)) {
				styler.SetLevel(lineCurrent, lev);
			}
			lineCurrent++;
			levelCurrent = levelNext;
			levelMinCurrent = levelCurrent;
			visibleChars = 0;
		}
	}
}

LexerModule lmVerilog(SCLEX_VERILOG, ColouriseVerilogDoc, "verilog", FoldVerilogDoc);
