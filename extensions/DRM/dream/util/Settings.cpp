/******************************************************************************\
 * Technische Universitaet Darmstadt, Institut fuer Nachrichtentechnik
 * Copyright (c) 2001-2007
 *
 * Author(s):
 *	Volker Fischer, Tomi Manninen, Stephane Fillod, Robert Kesterson,
 *	Andrea Russo, Andrew Murphy
 *
 * Description:
 *
 * 10/01/2007
 *  - parameters for rsci by Andrew Murphy
 * 07/27/2004
 *  - included stlini routines written by Robert Kesterson
 * 04/15/2004 Tomi Manninen, Stephane Fillod
 *  - Hamlib
 * 10/03/2003 Tomi Manninen / OH2BNS
 *  - Initial (basic) code for command line argument parsing (argv)
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
\******************************************************************************/

#include "Settings.h"
#include "printf.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
using namespace std;
#include "FileTyper.h"

/* Implementation *************************************************************/

CIniFile::CIniFile()
{
}

CIniFile::~CIniFile()
{
}

CSettings::CSettings():CIniFile()
{
}

CSettings::~CSettings()
{
}

void
CSettings::Load(int argc, char **argv)
{
	/* First load settings from init-file and then parse command line arguments.
	   The command line arguments overwrite settings in init-file! */
	(void)LoadIni(DREAM_INIT_FILE_NAME);
	ParseArguments(argc, argv);
}

void
CSettings::Save()
{
	/* Just write settings in init-file */
	SaveIni(DREAM_INIT_FILE_NAME);
}

string
CSettings::Get(const string & section, const string & key, const string & def) const
{
	return GetIniSetting(section, key, def);
}

void
CSettings::Put(const string & section, const string & key, const string& value)
{
	PutIniSetting(section, key, value);
}

bool
CSettings::Get(const string & section, const string & key, const bool def) const
{
	return GetIniSetting(section, key, def?"1":"0")=="1";
}

void CSettings::Put(const string& section, const string& key, const bool value)
{
	PutIniSetting(section, key, value?"1":"0");
}

int
CSettings::Get(const string & section, const string & key, const int def) const
{
	const string strGetIni = GetIniSetting(section, key);

	/* Check if it is a valid parameter */
	if (strGetIni.empty())
		return def;

	stringstream s(strGetIni);
	int iValue;
	s >> iValue;
	return iValue;
}

void
CSettings::Put(const string & section, const string & key, const int value)
{
	stringstream s;
	s << value;
	PutIniSetting(section, key, s.str());
}

_REAL
CSettings::Get(const string & section, const string & key, const _REAL def) const
{
	string s = GetIniSetting(section, key, "");
	if(s != "")
	{
		stringstream ss(s);
		_REAL rValue;
		ss >> rValue;
		return rValue;
	}
	return def;
}

void
CSettings::Put(const string & section, const string & key, const _REAL value)
{
	stringstream s;
	s << setiosflags(ios::left);
	s << setw(11);
	s << setiosflags(ios::fixed);
	s << setprecision(7);
	s << value;
	PutIniSetting(section, key, s.str());
}

void
CSettings::Get(const string& section, CWinGeom& value) const
{
	value.iXPos = Get(section, "x", 0);
	value.iYPos = Get(section, "y", 0);
	value.iWSize = Get(section, "width", 0);
	value.iHSize = Get(section, "height", 0);
}

void
CSettings::Put(const string& section, const CWinGeom& value)
{
	stringstream s;
	s << value.iXPos;
	PutIniSetting(section, "x", s.str());
	s.str("");
	s << value.iYPos;
	PutIniSetting(section, "y", s.str());
	s.str("");
	s << value.iWSize;
	PutIniSetting(section, "width", s.str());
	s.str("");
	s << value.iHSize;
	PutIniSetting(section, "height", s.str());
	s.str("");
}

int
CSettings::IsReceiver(const char *argv0)
{
#ifdef EXECUTABLE_NAME
	/* Give the possibility to launch directly dream transmitter
	   with a symbolic link to the executable, a 't' need to be 
	   appended to the symbolic link name */
# define _xstr(s) _str(s)
# define _str(s) #s
# ifndef _WIN32
	const int pathseparator = '/';
# else
	const int pathseparator = '\\';
# endif
	const char *str = strrchr(argv0, pathseparator);
	return strcmp(str ? str+1 : argv0, _xstr(EXECUTABLE_NAME) "t") != 0;
#else
	(void)argv0;
	return true;
#endif
}

void
CSettings::FileArg(const string& str)
{
    // Identify the type of file
    FileTyper::type t = FileTyper::resolve(str);
    if(FileTyper::is_rxstat(t))
    {
		// it's an RSI or MDI input file
		Put("command", "rsiin", str);
	}
	else
	{
		// its an I/Q or I/F file
		Put("command", "fileio", str);
	}
}
/* Command line argument parser ***********************************************/
void
CSettings::ParseArguments(int argc, char **argv)
{
	bool bIsReceiver;
	_REAL rArgument;
	string strArgument;
	string rsiOutProfile="A";

	bIsReceiver = IsReceiver(argv[0]);

	/* QT docu: argv()[0] is the program name, argv()[1] is the first
	   argument and argv()[argc()-1] is the last argument.
	   Start with first argument, therefore "i = 1" */
	if (bIsReceiver)
	{
		for (int i = 1; i < argc; i++)
		{
			/* DRM transmitter mode flag ---------------------------------------- */
			if (GetFlagArgument(argc, argv, i, "-t", "--transmitter"))
			{
				bIsReceiver = false;
				break;
			}
		}
	}

	const char* ReceiverTransmitter = bIsReceiver ? "Receiver" : "Transmitter";
	Put("command", "mode", bIsReceiver ? string("receive") : string("transmit"));

	for (int i = 1; i < argc; i++)
	{
	    /* "-X true" argument from lldb on re-runs */
		if (GetStringArgument(argc, argv, i, "-X", "", strArgument))
			continue;

	    /* "--" argument from lldb on re-runs */
		if (GetFlagArgument(argc, argv, i, "--", ""))
			continue;

		if (GetFlagArgument(argc, argv, i, "-t", "--transmitter"))
			continue;

		/* DRM transmitter mode flag ---------------------------------------- */
		bool no_console;
		if ((no_console = GetFlagArgument(argc, argv, i, "--noconsole", ""))) {
	        Put("command", "no_console", no_console? true : false);
			continue;
		}

		/* Sample rate ------------------------------------------------------ */
		if (GetNumericArgument(argc, argv, i, "-R", "--samplerate",
							   -1e9, +1e9, rArgument))
		{
			Put(ReceiverTransmitter, "samplerateaud", int (rArgument));
			Put(ReceiverTransmitter, "sampleratesig", int (rArgument));
			continue;
		}

		/* Audio sample rate ------------------------------------------------ */
		if (GetNumericArgument(argc, argv, i, "--audsrate", "--audsrate",
							   -1e9, +1e9, rArgument))
		{
			Put(ReceiverTransmitter, "samplerateaud", int (rArgument));
			continue;
		}

		/* Signal sample rate ------------------------------------------------ */
		if (GetNumericArgument(argc, argv, i, "--sigsrate", "--sigsrate",
							   -1e9, +1e9, rArgument))
		{
			Put(ReceiverTransmitter, "sampleratesig", int (rArgument));
			continue;
		}

		/* Sound In device -------------------------------------------------- */
		if (GetStringArgument(argc, argv, i, "-I", "--snddevin",
							  strArgument))
		{
			Put(ReceiverTransmitter, "snddevin", strArgument);
			continue;
		}

		/* Sound Out device ------------------------------------------------- */
		if (GetStringArgument(argc, argv, i, "-O", "--snddevout",
							  strArgument))
		{
			Put(ReceiverTransmitter, "snddevout", strArgument);
			continue;
		}

		/* Signal upscale ratio --------------------------------------------- */
		if (GetNumericArgument(argc, argv, i, "-U", "--sigupratio",
							  1, 2, rArgument))
		{
			Put("Receiver", "sigupratio", rArgument);
			continue;
		}

		/* Flip spectrum flag ----------------------------------------------- */
		if (GetNumericArgument(argc, argv, i, "-p", "--flipspectrum",
							   0, 1, rArgument))
		{
			Put("Receiver", "flipspectrum", int (rArgument));
			continue;
		}

		/* Mute audio flag -------------------------------------------------- */
		if (GetNumericArgument(argc, argv, i, "-m", "--muteaudio",
							   0, 1, rArgument))
		{
			Put("Receiver", "muteaudio", int (rArgument));
			continue;
		}

		/* Reverb audio flag ------------------------------------------------ */
		if (GetNumericArgument(argc, argv, i, "-b", "--reverb",
							   0, 1, rArgument))
		{
			Put("Receiver", "reverb", int (rArgument));
			continue;
		}

		/* Bandpass filter flag --------------------------------------------- */
		if (GetNumericArgument(argc, argv, i, "-F", "--filter",
							   0, 1, rArgument))
		{
			Put("Receiver", "filter", int (rArgument));
			continue;
		}

		/* Modified metrics flag -------------------------------------------- */
		if (GetNumericArgument(argc, argv, i, "-D", "--modmetric",
							   0, 1, rArgument))
		{
			Put("Receiver", "modmetric", int (rArgument));
			continue;
		}

		/* Do not use sound card, read from file ---------------------------- */
		if (GetStringArgument(argc, argv, i, "-f", "--fileio",
							  strArgument))
		{
			FileArg(strArgument);
			continue;
		}

		/* Write output data to file as WAV --------------------------------- */
		if (GetStringArgument(argc, argv, i, "-w", "--writewav",
							  strArgument))
		{
			Put("command", "writewav", strArgument);
			continue;
		}

		/* Number of iterations for MLC setting ----------------------------- */
		if (GetNumericArgument(argc, argv, i, "-i", "--mlciter", 0,
							   MAX_NUM_MLC_IT, rArgument))
		{
			Put("Receiver", "mlciter", int (rArgument));
			continue;
		}

		/* Sample rate offset start value ----------------------------------- */
		if (GetNumericArgument(argc, argv, i, "-s", "--sampleoff",
							   MIN_SAM_OFFS_INI, MAX_SAM_OFFS_INI,
							   rArgument))
		{
			Put("Receiver", "sampleoff", rArgument);
			continue;
		}

		/* Frequency acquisition search window size ------------------------- */
		if (GetNumericArgument(argc, argv, i, "-S", "--fracwinsize", -1,
							   MAX_FREQ_AQC_SE_WIN_SZ, rArgument))
		{
			Put("command", "fracwinsize", rArgument);
			continue;
		}

		/* Frequency acquisition search window center ----------------------- */
		if (GetNumericArgument(argc, argv, i, "-E", "--fracwincent", -1,
							   MAX_FREQ_AQC_SE_WIN_CT, rArgument))
		{
			Put("command", "fracwincent", rArgument);
			continue;
		}

		/* Input channel selection ------------------------------------------ */
		if (GetNumericArgument(argc, argv, i, "-c", "--inchansel", 0,
							   MAX_VAL_IN_CHAN_SEL, rArgument))
		{
			Put("Receiver", "inchansel", (int) rArgument);
			continue;
		}

		/* Output channel selection ----------------------------------------- */
		if (GetNumericArgument(argc, argv, i, "-u", "--outchansel", 0,
							   MAX_VAL_OUT_CHAN_SEL, rArgument))
		{
			Put("Receiver", "outchansel", (int) rArgument);
			continue;
		}

		/* Wanted RF Frequency   -------------------------------------------- */
		if (GetNumericArgument(argc, argv, i, "-r", "--frequency", 0,
							   MAX_RF_FREQ, rArgument))
		{
			Put("Receiver", "frequency", (int) rArgument);
			continue;
		}

		/* if 0 then only measure PSD when RSCI in use otherwise always measure it ---- */
		if (GetNumericArgument(argc, argv, i, "--enablepsd", "--enablepsd", 0, 1,
							   rArgument))
		{
			Put("Receiver", "measurepsdalways", (int) rArgument);
			continue;
		}

		/* Enable/Disable epg decoding -------------------------------------- */
//		if (GetNumericArgument(argc, argv, i, "-e", "--decodeepg", 0,
//							   1, rArgument))
//		{
//			Put("EPG", "decodeepg", (int) rArgument);
//			continue;
//		}

		/* MDI out address -------------------------------------------------- */
		if (GetStringArgument(argc, argv, i, "--mdiout", "--mdiout",
							  strArgument))
		{
			cerr <<
				"content server mode not implemented yet, perhaps you wanted rsiout"
				<< endl;
			continue;
		}

		/* MDI in address --------------------------------------------------- */
		if (GetStringArgument(argc, argv, i, "--mdiin", "--mdiin",
							  strArgument))
		{
			cerr <<
				"modulator mode not implemented yet, perhaps you wanted rsiin"
				<< endl;
			continue;
		}

		/* RSCI status output profile --------------------------------------- */
		if (GetStringArgument(argc, argv, i, "--rsioutprofile", "--rsioutprofile",
							  strArgument))
		{
			rsiOutProfile = strArgument;
			continue;
		}

		/* RSCI status out address ------------------------------------------ */
		if (GetStringArgument(argc, argv, i, "--rsiout", "--rsiout",
							  strArgument))
		{
			string s = Get("command", "rsiout", string(""));
			if(s == "")
				Put("command", "rsiout", rsiOutProfile+":"+strArgument);
			else
				Put("command", "rsiout", s+" "+rsiOutProfile+":"+strArgument);
			continue;
		}

		/* RSCI status in address ------------------------------------------- */
		if (GetStringArgument(argc, argv, i, "--rsiin", "--rsiin",
							  strArgument))
		{
			Put("command", "rsiin", strArgument);
			continue;
		}

		/* RSCI control out address ----------------------------------------- */
		if (GetStringArgument(argc, argv, i, "--rciout", "--rciout",
							  strArgument))
		{
			Put("command", "rciout", strArgument);
			continue;
		}

		/* OPH: RSCI control in address ------------------------------------- */
		if (GetStringArgument(argc, argv, i, "--rciin", "--rciin",
							  strArgument))
		{
			string s = Get("command", "rciin", string(""));
			if(s == "")
				Put("command", "rciin", strArgument);
			else
				Put("command", "rciin", s+" "+strArgument);
			continue;
		}

        if (GetNumericArgument(argc, argv, i, "-permissive",
                "--permissive", 0, 1, rArgument))
        {
            Put("command", "permissive", (int) rArgument);
            continue;
        }

		if (GetStringArgument (argc, argv, i,
				"--rsirecordprofile", "--rsirecordprofile", strArgument))
		{
			Put("command", "rsirecordprofile", strArgument);
			continue;
		}

		if (GetStringArgument (argc, argv, i,
				"--rsirecordtype", "--rsirecordtype", strArgument))
		{
			Put("command", "rsirecordtype", strArgument);
			continue;
		}

		if (GetNumericArgument (argc, argv, i,
				"--recordiq", "--recordiq", 0, 1, rArgument))
		{
			Put("command", "recordiq", int (rArgument));
			continue;
		}

		/* invoke test functionality ---------------------------------------- */
		if (GetStringArgument(argc, argv, i, "--test", "--test", strArgument))
		{
			Put("command", "test", strArgument);
			continue;
		}


		/* Help (usage) flag ------------------------------------------------ */
		if ((!strcmp(argv[i], "--help")) ||
			(!strcmp(argv[i], "-h")) || (!strcmp(argv[i], "-?")))
		{
			Put("command", "mode", "help");
			continue;
		}

		/* not an option ---------------------------------------------------- */
		if(argv[i][0] != '-')
		{
			FileArg(string(argv[i]));
			continue;
		}

		/* Unknown option --------------------------------------------------- */
		cerr << argv[0] << ": ";
		cerr << "Unknown option '" << argv[i] << "' -- use '--help' for help"
			<< endl;

		kiwi_exit(1);
	}
}

/*
 Option argument:
  <b> boolean (0=off, 1=on)
  <n> integer number
  <r> real number
  <s> string
*/

const char *
CSettings::UsageArguments()
{
	/* The text below must be translatable */
	return
		"Usage: $EXECNAME [option [argument]] | [input file]\n"
		"\n"
		"Recognized options:\n"
		"  -t, --transmitter            DRM transmitter mode\n"
		"  -x, --noconsole              temp disable console mode (if configured)\n"
		"  -p <b>, --flipspectrum <b>   flip input spectrum (0: off; 1: on)\n"
		"  -i <n>, --mlciter <n>        number of MLC iterations (allowed range: 0...4 default: 1)\n"
		"  -s <r>, --sampleoff <r>      sample rate offset initial value [Hz] (allowed range: -200.0...200.0)\n"
		"  -m <b>, --muteaudio <b>      mute audio output (0: off; 1: on)\n"
		"  -b <b>, --reverb <b>         audio reverberation on drop-out (0: off; 1: on)\n"
		"  -f <s>, --fileio <s>         disable sound card, use file <s> instead\n"
		"  -w <s>, --writewav <s>       write output to wave file\n"
		"  -S <r>, --fracwinsize <r>    freq. acqu. search window size [Hz] (-1.0: sample rate / 2 (default))\n"
		"  -E <r>, --fracwincent <r>    freq. acqu. search window center [Hz] (-1.0: sample rate / 4 (default))\n"
		"  -F <b>, --filter <b>         apply bandpass filter (0: off; 1: on)\n"
		"  -D <b>, --modmetric <b>      enable modified metrics (0: off; 1: on)\n"
		"  -c <n>, --inchansel <n>      input channel selection\n"
		"                               0: left channel;                     1: right channel;\n"
		"                               2: mix both channels (default);      3: subtract right from left;\n"
		"                               4: I / Q input positive;             5: I / Q input negative;\n"
		"                               6: I / Q input positive (0 Hz IF);   7: I / Q input negative (0 Hz IF)\n"
		"                               8: I / Q input positive split;       9: I / Q input negative split\n"
		"  -u <n>, --outchansel <n>     output channel selection\n"
		"                               0: L -> L, R -> R (default);   1: L -> L, R muted;   2: L muted, R -> R\n"
		"                               3: mix -> L, R muted;          4: L muted, mix -> R\n"
		"  --enablepsd <b>              if 0 then only measure PSD when RSCI in use otherwise always measure it\n"
		"  --mdiout <s>                 MDI out address format [IP#:]IP#:port (for Content Server)\n"
		"  --mdiin  <s>                 MDI in address (for modulator) [[IP#:]IP:]port\n"
		"  --rsioutprofile <s>          MDI/RSCI output profile: A|B|C|D|Q|M\n"
		"  --rsiout <s>                 MDI/RSCI output address format [IP#:]IP#:port (prefix address with 'p' to enable the simple PFT)\n"
		"  --rsiin <s>                  MDI/RSCI input address format [[IP#:]IP#:]port\n"
		"  --rciout <s>                 RSCI Control output format IP#:port\n"
		"  --rciin <s>                  RSCI Control input address number format [IP#:]port\n"
		"  --rsirecordprofile <s>       RSCI recording profile: A|B|C|D|Q|M\n"
		"  --rsirecordtype <s>          RSCI recording file type: raw|ff|pcap\n"
		"  --recordiq <b>               enable/disable recording an I/Q file\n"
		"  --permissive <b>             enable decoding of bad RSCI frames (0: off; 1: on)\n"
		"  -R <n>, --samplerate <n>     set audio and signal sound card sample rate [Hz]\n"
		"  --audsrate <n>               set audio sound card sample rate [Hz] (allowed range: 8000...192000)\n"
		"  --sigsrate <n>               set signal sound card sample rate [Hz] (allowed values: 24000, 48000, 96000, 192000)\n"
		"  -I <s>, --snddevin <s>       set sound in device\n"
		"  -O <s>, --snddevout <s>      set sound out device\n"
		"  -U <n>, --sigupratio <n>     set signal upscale ratio (allowed values: 1, 2)\n"
		"  --test <n>                   if 1 then some test setup will be done\n"
		"  -h, -?, --help               this help text\n"
		"\n"
		"Example: $EXECNAME -p --sampleoff -0.23 -i 2"
		"";
}

bool
CSettings::GetFlagArgument(int, char **argv, int &i,
						   string strShortOpt, string strLongOpt)
{
	if ((!strShortOpt.compare(argv[i])) || (!strLongOpt.compare(argv[i])))
		return true;
	else
		return false;
}

bool
CSettings::GetStringArgument(int argc, char **argv, int &i,
							 string strShortOpt, string strLongOpt,
							 string & strArg)
{
	if ((!strShortOpt.compare(argv[i])) || (!strLongOpt.compare(argv[i])))
	{
		if (++i >= argc)
		{
			cerr << argv[0] << ": ";
			cerr << "'" << strLongOpt << "' needs a string argument" << endl;
			kiwi_exit(1);
		}

		strArg = argv[i];

		return true;
	}
	else
		return false;
}

bool
CSettings::GetNumericArgument(int argc, char **argv, int &i,
							  string strShortOpt, string strLongOpt,
							  _REAL rRangeStart, _REAL rRangeStop,
							  _REAL & rValue)
{
	if ((!strShortOpt.compare(argv[i])) || (!strLongOpt.compare(argv[i])))
	{
		if (++i >= argc)
		{
			cerr << argv[0] << ": ";
			cerr << "'" << strLongOpt << "' needs a numeric argument between "
				<< rRangeStart << " and " << rRangeStop << endl;
			kiwi_exit(1);
		}

		char *p;
		rValue = strtod(argv[i], &p);
		if (*p || rValue < rRangeStart || rValue > rRangeStop)
		{
			cerr << argv[0] << ": ";
			cerr << "'" << strLongOpt << "' needs a numeric argument between "
				<< rRangeStart << " and " << rRangeStop << endl;
			kiwi_exit(1);
		}

		return true;
	}
	else
		return false;
}

/* INI File routines using the STL ********************************************/
/* The following code was taken from "INI File Tools (STLINI)" written by
   Robert Kesterson in 1999. The original files are stlini.cpp and stlini.h.
   The homepage is http://robertk.com/source

   Copyright August 18, 1999 by Robert Kesterson */

string
CIniFile::GetIniSetting(const string& section,
	 const string& key, const string& defaultval) const
{
	string result(defaultval);
	const_cast<CMutex*>(&Mutex)->Lock();
	INIFile::const_iterator iSection = ini.find(section);
	if (iSection != ini.end())
	{
		INISection::const_iterator apair = iSection->second.find(key);
		if (apair != iSection->second.end())
		{
			result = apair->second;
		}
	}
	const_cast<CMutex*>(&Mutex)->Unlock();
	return result;
}

void
CIniFile::PutIniSetting(const string& section, const string& key, const string& value)
{
	Mutex.Lock();

	/* null key is ok and empty value is ok but empty both is not useful */
	if(key != "" || value != "")
		ini[section][key]=value;

	Mutex.Unlock();
}

bool
CIniFile::LoadIni(const char *filename)
{
	string section;
	ifstream file(filename);

	bool ok = false;

	while (file.good())
	{
		ok = true;

		if(file.peek() == '[')
		{
			file.ignore(); // read '['
			getline(file, section, ']');
			file.ignore(80, '\n'); // read eol
		}
		else if(file.peek() == '\r')
		{
			file.ignore(); // read '\r'
		}
		else if(file.peek() == '\n')
		{
			file.ignore(10, '\n'); // read eol
		}
		else
		{
			string key,value;
			getline(file, key, '=');
			getline(file, value);
			int n = int(value.length())-1;
			if(n >= 0)
			{
				if(value[n] == '\r') // remove CR if file has DOS line endings
					PutIniSetting(section, key, value.substr(0,n));
				else
					PutIniSetting(section, key, value);
			}
		}
	}
	return ok;
}

void
CIniFile::SaveIni(const char *filename) const
{
	fstream file(filename, std::ios::out);
	SaveIni(file);
	file.close();
}

void
CIniFile::SaveIni(ostream& file) const
{
	bool bFirstSection = true;	/* Init flag */
	if (!file.good())
		return;

	/* Just iterate the hashes and values and dump them to the stream */
	for(INIFile::const_iterator section = ini.begin(); section != ini.end(); section++)
	{
		if (section->first != "command")
		{
			if (section->first > "")
			{
				if (bFirstSection)
				{
					/* Don't put a newline at the beginning of the first section */
					file << "[" << section->first << "]" << std::endl;

					/* Reset flag */
					bFirstSection = false;
				}
				else
					file << std::endl << "[" << section-> first << "]" << std::endl;
			}

			INISection::const_iterator pair = section->second.begin();

			while (pair != section->second.end())
			{
				if (pair->second > "")
					file << pair->first << "=" << pair->second << std::endl;
				else
					file << pair->first << "=" << std::endl;
				pair++;
			}
		}
	}
}

/* Return true or false depending on whether the first string is less than the
   second */
bool
StlIniCompareStringNoCase::operator() (const string & x, const string & y) const
{
#ifdef _WIN32
	return (_stricmp(x.c_str(), y.c_str()) < 0) ? true : false;
#else
	return (strcasecmp(x.c_str(), y.c_str()) < 0) ? true : false;
#endif /* strcasecmp */
}
