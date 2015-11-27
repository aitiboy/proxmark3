//-----------------------------------------------------------------------------
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Low frequency T55xx commands
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include "proxmark3.h"
#include "ui.h"
#include "graph.h"
#include "cmdmain.h"
#include "cmdparser.h"
#include "cmddata.h"
#include "cmdlf.h"
#include "cmdlft55xx.h"
#include "util.h"
#include "data.h"
#include "lfdemod.h"
#include "../common/crc.h"
#include "../common/iso14443crc.h"
#include "cmdhf14a.h"

#define T55x7_CONFIGURATION_BLOCK 0x00
#define T55x7_PAGE0 0x00
#define T55x7_PAGE1 0x01
#define REGULAR_READ_MODE_BLOCK 0xFF

// Default configuration
t55xx_conf_block_t config = { .modulation = DEMOD_ASK, .inverted = FALSE, .offset = 0x00, .block0 = 0x00, .Q5 = FALSE };

t55xx_conf_block_t Get_t55xx_Config(){
	return config;
}
void Set_t55xx_Config(t55xx_conf_block_t conf){
	config = conf;
}

int usage_t55xx_config(){
	PrintAndLog("Usage: lf t55xx config [d <demodulation>] [i 1] [o <offset>] [Q5]");
	PrintAndLog("Options:");
	PrintAndLog("       h                        This help");
	PrintAndLog("       b <8|16|32|40|50|64|100|128>  Set bitrate");
	PrintAndLog("       d <FSK|FSK1|FSK1a|FSK2|FSK2a|ASK|PSK1|PSK2|NRZ|BI|BIa>  Set demodulation FSK / ASK / PSK / NRZ / Biphase / Biphase A");
	PrintAndLog("       i [1]                         Invert data signal, defaults to normal");
	PrintAndLog("       o [offset]                    Set offset, where data should start decode in bitstream");
	PrintAndLog("       Q5                            Set as Q5(T5555) chip instead of T55x7");
	PrintAndLog("");
	PrintAndLog("Examples:");
	PrintAndLog("      lf t55xx config d FSK          - FSK demodulation");
	PrintAndLog("      lf t55xx config d FSK i 1      - FSK demodulation, inverse data");
	PrintAndLog("      lf t55xx config d FSK i 1 o 3  - FSK demodulation, inverse data, offset=3,start from position 3 to decode data");
	PrintAndLog("");
	return 0;
}
int usage_t55xx_read(){
	PrintAndLog("Usage:  lf t55xx read [b <block>] [p <password>] <override_safety> <page1>");
	PrintAndLog("Options:");
	PrintAndLog("     b <block>    - block number to read. Between 0-7");
	PrintAndLog("     p <password> - OPTIONAL password (8 hex characters)");
	PrintAndLog("     o            - OPTIONAL override safety check");
	PrintAndLog("     1            - OPTIONAL read Page 1 instead of Page 0");
	PrintAndLog("     ****WARNING****");
	PrintAndLog("     Use of read with password on a tag not configured for a pwd");
	PrintAndLog("     can damage the tag");
	PrintAndLog("");
	PrintAndLog("Examples:");
	PrintAndLog("      lf t55xx read b 0              - read data from block 0");
	PrintAndLog("      lf t55xx read b 0 p feedbeef   - read data from block 0 password feedbeef");
	PrintAndLog("      lf t55xx read b 0 p feedbeef o - read data from block 0 password feedbeef safety check");
	PrintAndLog("");
	return 0;
}
int usage_t55xx_write(){
	PrintAndLog("Usage:  lf t55xx wr [b <block>] [d <data>] [p <password>] [1]");
	PrintAndLog("Options:");
	PrintAndLog("     b <block>    - block number to write. Between 0-7");
	PrintAndLog("     d <data>     - 4 bytes of data to write (8 hex characters)");
	PrintAndLog("     p <password> - OPTIONAL password 4bytes (8 hex characters)");
	PrintAndLog("     1            - OPTIONAL write Page 1 instead of Page 0");
	PrintAndLog("");
	PrintAndLog("Examples:");
	PrintAndLog("      lf t55xx wr b 3 d 11223344            - write 11223344 to block 3");
	PrintAndLog("      lf t55xx wr b 3 d 11223344 p feedbeef - write 11223344 to block 3 password feedbeef");
	PrintAndLog("");
	return 0;
}
int usage_t55xx_trace() {
	PrintAndLog("Usage:  lf t55xx trace [1]");
	PrintAndLog("Options:");
	PrintAndLog("     [graph buffer data]  - if set, use Graphbuffer otherwise read data from tag.");
	PrintAndLog("");
	PrintAndLog("Examples:");
	PrintAndLog("      lf t55xx trace");
	PrintAndLog("      lf t55xx trace 1");
	PrintAndLog("");
	return 0;
}
int usage_t55xx_info() {
	PrintAndLog("Usage:  lf t55xx info [1]");
	PrintAndLog("Options:");
	PrintAndLog("     [graph buffer data]  - if set, use Graphbuffer otherwise read data from tag.");
	PrintAndLog("");
	PrintAndLog("Examples:");
	PrintAndLog("      lf t55xx info");
	PrintAndLog("      lf t55xx info 1");
	PrintAndLog("");
	return 0;
}
int usage_t55xx_dump(){
	PrintAndLog("Usage:  lf t55xx dump <password> [o]");
	PrintAndLog("Options:");
	PrintAndLog("     <password>  - OPTIONAL password 4bytes (8 hex symbols)");
	PrintAndLog("     o           - OPTIONAL override, force pwd read despite danger to card");
	PrintAndLog("");
	PrintAndLog("Examples:");
	PrintAndLog("      lf t55xx dump");
	PrintAndLog("      lf t55xx dump feedbeef o");
	PrintAndLog("");
	return 0;
}
int usage_t55xx_detect(){
	PrintAndLog("Usage:  lf t55xx detect [1] [p <password>]");
	PrintAndLog("Options:");
	PrintAndLog("     1             - if set, use Graphbuffer otherwise read data from tag.");
	PrintAndLog("     p <password>  - OPTIONAL password (8 hex characters)");
	PrintAndLog("");
	PrintAndLog("Examples:");
	PrintAndLog("      lf t55xx detect");
	PrintAndLog("      lf t55xx detect 1");
	PrintAndLog("      lf t55xx detect p 11223344");
	PrintAndLog("");
	return 0;
}
int usage_t55xx_wakup(){
	PrintAndLog("Usage:  lf t55xx wakeup [h] p <password>");
	PrintAndLog("This commands send the Answer-On-Request command and leaves the readerfield ON afterwards.");
	PrintAndLog("Options:");
	PrintAndLog("     h             - this help");
	PrintAndLog("     p <password>  - password 4bytes (8 hex symbols)");
	PrintAndLog("");
	PrintAndLog("Examples:");
		PrintAndLog("      lf t55xx wakeup p 11223344  - send wakeup password");
	return 0;
}

static int CmdHelp(const char *Cmd);

void printT5xxHeader(uint8_t page){
	PrintAndLog("Reading Page %d:", page);	
	PrintAndLog("blk | hex data | binary");
	PrintAndLog("----+----------+---------------------------------");	
}

int CmdT55xxSetConfig(const char *Cmd) {

	uint8_t offset = 0;
	char modulation[5] = {0x00};
	char tmp = 0x00;
	uint8_t bitRate = 0;
	uint8_t rates[9] = {8,16,32,40,50,64,100,128,0};
	uint8_t cmdp = 0;
	config.Q5 = FALSE;
	bool errors = FALSE;
	while(param_getchar(Cmd, cmdp) != 0x00 && !errors)
	{
		tmp = param_getchar(Cmd, cmdp);
		switch(tmp)
		{
		case 'h':
		case 'H':
			return usage_t55xx_config();
		case 'b':
			errors |= param_getdec(Cmd, cmdp+1, &bitRate);
			if ( !errors){
				uint8_t i = 0;
				for (; i < 9; i++){
					if (rates[i]==bitRate) {
						config.bitrate = i;
						break;
					}
				}
				if (i==9) errors = TRUE;
			}
			cmdp+=2;
			break;
		case 'd':
			param_getstr(Cmd, cmdp+1, modulation);
			cmdp += 2;

			if ( strcmp(modulation, "FSK" ) == 0) {
				config.modulation = DEMOD_FSK;
			} else if ( strcmp(modulation, "FSK1" ) == 0) {
				config.modulation = DEMOD_FSK1;
				config.inverted=1;
			} else if ( strcmp(modulation, "FSK1a" ) == 0) {
				config.modulation = DEMOD_FSK1a;
				config.inverted=0;
			} else if ( strcmp(modulation, "FSK2" ) == 0) {
				config.modulation = DEMOD_FSK2;
				config.inverted=0;
			} else if ( strcmp(modulation, "FSK2a" ) == 0) {
				config.modulation = DEMOD_FSK2a;
				config.inverted=1;
			} else if ( strcmp(modulation, "ASK" ) == 0) {
				config.modulation = DEMOD_ASK;
			} else if ( strcmp(modulation, "NRZ" ) == 0) {
				config.modulation = DEMOD_NRZ;
			} else if ( strcmp(modulation, "PSK1" ) == 0) {
				config.modulation = DEMOD_PSK1;
			} else if ( strcmp(modulation, "PSK2" ) == 0) {
				config.modulation = DEMOD_PSK2;
			} else if ( strcmp(modulation, "PSK3" ) == 0) {
				config.modulation = DEMOD_PSK3;
			} else if ( strcmp(modulation, "BIa" ) == 0) {
				config.modulation = DEMOD_BIa;
				config.inverted=1;
			} else if ( strcmp(modulation, "BI" ) == 0) {
				config.modulation = DEMOD_BI;
				config.inverted=0;
			} else {
				PrintAndLog("Unknown modulation '%s'", modulation);
				errors = TRUE;
			}
			break;
		case 'i':
			config.inverted = param_getchar(Cmd,cmdp+1) == '1';
			cmdp+=2;
			break;
		case 'o':
			errors |= param_getdec(Cmd, cmdp+1, &offset);
			if ( !errors )
				config.offset = offset;
			cmdp+=2;
			break;
		case 'Q':
		case 'q':		
			config.Q5 = TRUE;
			cmdp++;
			break;
		default:
			PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
			errors = TRUE;
			break;
		}
	}

	// No args
	if (cmdp == 0) return printConfiguration( config );

	//Validations
	if (errors) return usage_t55xx_config();

	config.block0 = 0;
	return printConfiguration ( config );
}

int T55xxReadBlock(uint8_t block, bool page1, bool usepwd, bool override, uint32_t password){
	//Password mode
	if ( usepwd ) {
		// try reading the config block and verify that PWD bit is set before doing this!
		if ( !override ) {
			if ( !AquireData(T55x7_PAGE0, T55x7_CONFIGURATION_BLOCK, false, 0 ) ) return 0;
			if ( !tryDetectModulation() ) {
				PrintAndLog("Safety Check: Could not detect if PWD bit is set in config block. Exits.");
				return 0;
			} else {
				PrintAndLog("Safety Check: PWD bit is NOT set in config block. Reading without password...");	
				usepwd = false;
				page1 = false;
			}
		} else {
			PrintAndLog("Safety Check Overriden - proceeding despite risk");
		}
	}

	if (!AquireData(page1, block, usepwd, password) )	return 0;
	if (!DecodeT55xxBlock()) return 0;

	char blk[10]={0};
	sprintf(blk,"%d", block);
	printT55xxBlock(blk);	
	return 1;
}

int CmdT55xxReadBlock(const char *Cmd) {
	uint8_t block = REGULAR_READ_MODE_BLOCK;
	uint32_t password = 0; //default to blank Block 7
	bool usepwd = false;
	bool override = false;
	bool page1 = false;
	bool errors = false;
	uint8_t cmdp = 0;
	while(param_getchar(Cmd, cmdp) != 0x00 && !errors) {
		switch(param_getchar(Cmd, cmdp)) {
		case 'h':
		case 'H':
			return usage_t55xx_read();
		case 'b':
		case 'B':
			errors |= param_getdec(Cmd, cmdp+1, &block);
			cmdp += 2;
			break;
		case 'o':
		case 'O':
			override = true;
			cmdp++;
			break;
		case 'p':
		case 'P':
			password = param_get32ex(Cmd, cmdp+1, 0, 16);
			usepwd = true;
			cmdp += 2;
			break;
		case '1':
			page1 = true;
			cmdp++;
			break;
		default:
			PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
			errors = true;
			break;
		}
	}
	if (errors) return usage_t55xx_read();

	if (block > 7 && block != REGULAR_READ_MODE_BLOCK	) {
		PrintAndLog("Block must be between 0 and 7");
		return 0;
	}

	printT5xxHeader(page1);
	return T55xxReadBlock(block, page1, usepwd, override, password);
}

bool DecodeT55xxBlock(){
	
	char buf[30] = {0x00};
	char *cmdStr = buf;
	int ans = 0;
	uint8_t bitRate[8] = {8,16,32,40,50,64,100,128};
	DemodBufferLen = 0x00;

	switch( config.modulation ){
		case DEMOD_FSK:
			snprintf(cmdStr, sizeof(buf),"%d %d", bitRate[config.bitrate], config.inverted );
			ans = FSKrawDemod(cmdStr, FALSE);
			break;
		case DEMOD_FSK1:
		case DEMOD_FSK1a:
			snprintf(cmdStr, sizeof(buf),"%d %d 8 5", bitRate[config.bitrate], config.inverted );
			ans = FSKrawDemod(cmdStr, FALSE);
			break;
		case DEMOD_FSK2:
		case DEMOD_FSK2a:
			snprintf(cmdStr, sizeof(buf),"%d %d 10 8", bitRate[config.bitrate], config.inverted );
			ans = FSKrawDemod(cmdStr, FALSE);
			break;
		case DEMOD_ASK:
			snprintf(cmdStr, sizeof(buf),"%d %d 1", bitRate[config.bitrate], config.inverted );
			ans = ASKDemod(cmdStr, FALSE, FALSE, 1);
			break;
		case DEMOD_PSK1:
			// skip first 160 samples to allow antenna to settle in (psk gets inverted occasionally otherwise)
			CmdLtrim("160");
			snprintf(cmdStr, sizeof(buf),"%d %d 6", bitRate[config.bitrate], config.inverted );
			ans = PSKDemod(cmdStr, FALSE);
			break;
		case DEMOD_PSK2: //inverted won't affect this
		case DEMOD_PSK3: //not fully implemented
			// skip first 160 samples to allow antenna to settle in (psk gets inverted occasionally otherwise)
			CmdLtrim("160");
			snprintf(cmdStr, sizeof(buf),"%d 0 6", bitRate[config.bitrate] );
			ans = PSKDemod(cmdStr, FALSE);
			psk1TOpsk2(DemodBuffer, DemodBufferLen);
			break;
		case DEMOD_NRZ:
			snprintf(cmdStr, sizeof(buf),"%d %d 1", bitRate[config.bitrate], config.inverted );
			ans = NRZrawDemod(cmdStr, FALSE);
			break;
		case DEMOD_BI:
		case DEMOD_BIa:
			snprintf(cmdStr, sizeof(buf),"0 %d %d 1", bitRate[config.bitrate], config.inverted );
			ans = ASKbiphaseDemod(cmdStr, FALSE);
			break;
		default:
			return FALSE;
	}
	return (bool) ans;
}

int CmdT55xxDetect(const char *Cmd){
	bool errors = FALSE;
	bool useGB = FALSE;
	bool usepwd = FALSE;
	uint32_t password = 0;
	uint8_t cmdp = 0;

	while(param_getchar(Cmd, cmdp) != 0x00 && !errors) {
		switch(param_getchar(Cmd, cmdp)) {
		case 'h':
		case 'H':
			return usage_t55xx_detect();
		case 'p':
		case 'P':
			password = param_get32ex(Cmd, cmdp+1, 0, 16);
			usepwd = TRUE;
			cmdp += 2;
			break;
		case '1':
			// use Graphbuffer data
			useGB = TRUE;
			cmdp++;
			break;
		default:
			PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
			errors = true;
			break;
		}
	}
	if (errors) return usage_t55xx_detect();
	
	if ( !useGB) {
		if ( !AquireData(T55x7_PAGE0, T55x7_CONFIGURATION_BLOCK, usepwd, password) )
			return 0;
	}
	
	if ( !tryDetectModulation() )
		PrintAndLog("Could not detect modulation automatically. Try setting it manually with \'lf t55xx config\'");

	return 1;
}

// detect configuration?
bool tryDetectModulation(){
	uint8_t hits = 0;
	t55xx_conf_block_t tests[15];
	int bitRate=0;
	uint8_t fc1 = 0, fc2 = 0, clk=0;
	save_restoreGB(1);

	if (GetFskClock("", FALSE, FALSE)){ 
		fskClocks(&fc1, &fc2, &clk, FALSE);
		if ( FSKrawDemod("0 0", FALSE) && test(DEMOD_FSK, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)){
			tests[hits].modulation = DEMOD_FSK;
			if (fc1==8 && fc2 == 5)
				tests[hits].modulation = DEMOD_FSK1a;
 			else if (fc1==10 && fc2 == 8)
				tests[hits].modulation = DEMOD_FSK2;
			tests[hits].bitrate = bitRate;
			tests[hits].inverted = FALSE;
			tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
			++hits;
		}
		if ( FSKrawDemod("0 1", FALSE) && test(DEMOD_FSK, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)) {
			tests[hits].modulation = DEMOD_FSK;
			if (fc1 == 8 && fc2 == 5)
				tests[hits].modulation = DEMOD_FSK1;
			else if (fc1 == 10 && fc2 == 8)
				tests[hits].modulation = DEMOD_FSK2a;

			tests[hits].bitrate = bitRate;
			tests[hits].inverted = TRUE;
			tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
			++hits;
		}
	} else {
		clk = GetAskClock("", FALSE, FALSE);
		if (clk>0) {
			if ( ASKDemod("0 0 1", FALSE, FALSE, 1) && test(DEMOD_ASK, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)) {
				tests[hits].modulation = DEMOD_ASK;
				tests[hits].bitrate = bitRate;
				tests[hits].inverted = FALSE;
				tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
				++hits;
			}
			if ( ASKDemod("0 1 1", FALSE, FALSE, 1)  && test(DEMOD_ASK, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)) {
				tests[hits].modulation = DEMOD_ASK;
				tests[hits].bitrate = bitRate;
				tests[hits].inverted = TRUE;
				tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
				++hits;
			}
			if ( ASKbiphaseDemod("0 0 0 2", FALSE) && test(DEMOD_BI, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5) ) {
				tests[hits].modulation = DEMOD_BI;
				tests[hits].bitrate = bitRate;
				tests[hits].inverted = FALSE;
				tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
				++hits;
			}
			if ( ASKbiphaseDemod("0 0 1 2", FALSE) && test(DEMOD_BIa, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5) ) {
				tests[hits].modulation = DEMOD_BIa;
				tests[hits].bitrate = bitRate;
				tests[hits].inverted = TRUE;
				tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
				++hits;
			}
		}
		//undo trim from ask
		save_restoreGB(0);
		clk = GetNrzClock("", FALSE, FALSE);
		if (clk>0) {
			if ( NRZrawDemod("0 0 1", FALSE)  && test(DEMOD_NRZ, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)) {
				tests[hits].modulation = DEMOD_NRZ;
				tests[hits].bitrate = bitRate;
				tests[hits].inverted = FALSE;
				tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
				++hits;
			}

			if ( NRZrawDemod("0 1 1", FALSE)  && test(DEMOD_NRZ, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)) {
				tests[hits].modulation = DEMOD_NRZ;
				tests[hits].bitrate = bitRate;
				tests[hits].inverted = TRUE;
				tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
				++hits;
			}
		}
		
		//undo trim from nrz
		save_restoreGB(0);
		// skip first 160 samples to allow antenna to settle in (psk gets inverted occasionally otherwise)
		CmdLtrim("160");
		clk = GetPskClock("", FALSE, FALSE);
		if (clk>0) {
			if ( PSKDemod("0 0 6", FALSE) && test(DEMOD_PSK1, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)) {
				tests[hits].modulation = DEMOD_PSK1;
				tests[hits].bitrate = bitRate;
				tests[hits].inverted = FALSE;
				tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
				++hits;
			}
			if ( PSKDemod("0 1 6", FALSE) && test(DEMOD_PSK1, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)) {
				tests[hits].modulation = DEMOD_PSK1;
				tests[hits].bitrate = bitRate;
				tests[hits].inverted = TRUE;
				tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
				++hits;
			}
			// PSK2 - needs a call to psk1TOpsk2.
			if ( PSKDemod("0 0 6", FALSE)) {
				psk1TOpsk2(DemodBuffer, DemodBufferLen);
				if (test(DEMOD_PSK2, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)){
					tests[hits].modulation = DEMOD_PSK2;
					tests[hits].bitrate = bitRate;
					tests[hits].inverted = FALSE;
					tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
					++hits;
				}
			} // inverse waves does not affect this demod
			// PSK3 - needs a call to psk1TOpsk2.
			if ( PSKDemod("0 0 6", FALSE)) {
				psk1TOpsk2(DemodBuffer, DemodBufferLen);
				if (test(DEMOD_PSK3, &tests[hits].offset, &bitRate, clk, &tests[hits].Q5)){
					tests[hits].modulation = DEMOD_PSK3;
					tests[hits].bitrate = bitRate;
					tests[hits].inverted = FALSE;
					tests[hits].block0 = PackBits(tests[hits].offset, 32, DemodBuffer);
					++hits;
				}
			} // inverse waves does not affect this demod
		}
	}	
	save_restoreGB(0);	
	if ( hits == 1) {
		config.modulation = tests[0].modulation;
		config.bitrate = tests[0].bitrate;
		config.inverted = tests[0].inverted;
		config.offset = tests[0].offset;
		config.block0 = tests[0].block0;
		printConfiguration( config );
		return TRUE;
	}
	
	if ( hits > 1) {
		PrintAndLog("Found [%d] possible matches for modulation.",hits);
		for(int i=0; i<hits; ++i){
			PrintAndLog("--[%d]---------------", i+1);
			printConfiguration( tests[i] );
		}
	}
	return FALSE;
}

bool testModulation(uint8_t mode, uint8_t modread){
	switch( mode ){
		case DEMOD_FSK:
			if (modread >= DEMOD_FSK1 && modread <= DEMOD_FSK2a) return TRUE;
			break;
		case DEMOD_ASK:
			if (modread == DEMOD_ASK) return TRUE;
			break;
		case DEMOD_PSK1:
			if (modread == DEMOD_PSK1) return TRUE;
			break;
		case DEMOD_PSK2:
			if (modread == DEMOD_PSK2) return TRUE;
			break;
		case DEMOD_PSK3:
			if (modread == DEMOD_PSK3) return TRUE;
			break;
		case DEMOD_NRZ:
			if (modread == DEMOD_NRZ) return TRUE;
			break;
		case DEMOD_BI:
			if (modread == DEMOD_BI) return TRUE;
			break;
		case DEMOD_BIa:
			if (modread == DEMOD_BIa) return TRUE;
			break;		
		default:
			return FALSE;
	}
	return FALSE;
}

bool testQ5Modulation(uint8_t	mode, uint8_t	modread){
	switch( mode ){
		case DEMOD_FSK:
			if (modread >= 4 && modread <= 5) return TRUE;
			break;
		case DEMOD_ASK:
			if (modread == 0) return TRUE;
			break;
		case DEMOD_PSK1:
			if (modread == 1) return TRUE;
			break;
		case DEMOD_PSK2:
			if (modread == 2) return TRUE;
			break;
		case DEMOD_PSK3:
			if (modread == 3) return TRUE;
			break;
		case DEMOD_NRZ:
			if (modread == 7) return TRUE;
			break;
		case DEMOD_BI:
			if (modread == 6) return TRUE;
			break;
		default:
			return FALSE;
	}
	return FALSE;
}

bool testQ5(uint8_t mode, uint8_t *offset, int *fndBitRate, uint8_t	clk){

	if ( DemodBufferLen < 64 ) return FALSE;
	uint8_t si = 0;
	for (uint8_t idx = 28; idx < 64; idx++){
		si = idx;
		if ( PackBits(si, 28, DemodBuffer) == 0x00 ) continue;

		uint8_t safer     = PackBits(si, 4, DemodBuffer); si += 4;     //master key
		uint8_t resv      = PackBits(si, 8, DemodBuffer); si += 8;
		// 2nibble must be zeroed.
		if (safer != 0x6) continue;
		if ( resv > 0x00) continue;
		//uint8_t	pageSel   = PackBits(si, 1, DemodBuffer); si += 1;
		//uint8_t fastWrite = PackBits(si, 1, DemodBuffer); si += 1;
		si += 1+1;
		int bitRate       = PackBits(si, 5, DemodBuffer)*2 + 2; si += 5;     //bit rate
		if (bitRate > 128 || bitRate < 8) continue;

		//uint8_t AOR       = PackBits(si, 1, DemodBuffer); si += 1;   
		//uint8_t PWD       = PackBits(si, 1, DemodBuffer); si += 1; 
		//uint8_t pskcr     = PackBits(si, 2, DemodBuffer); si += 2;  //could check psk cr
		//uint8_t inverse   = PackBits(si, 1, DemodBuffer); si += 1;
		si += 1+1+2+1;
		uint8_t modread   = PackBits(si, 3, DemodBuffer); si += 3;
		uint8_t maxBlk    = PackBits(si, 3, DemodBuffer); si += 3;
		//uint8_t ST        = PackBits(si, 1, DemodBuffer); si += 1;
		if (maxBlk == 0) continue;
		//test modulation
		if (!testQ5Modulation(mode, modread)) continue;
		if (bitRate != clk) continue;
		*fndBitRate = bitRate;
		*offset = idx;

		return TRUE;
	}
	return FALSE;
}

bool testBitRate(uint8_t readRate, uint8_t clk){
	uint8_t expected[] = {8, 16, 32, 40, 50, 64, 100, 128};
	if (expected[readRate] == clk)
		return true;

	return false;
}

bool test(uint8_t mode, uint8_t *offset, int *fndBitRate, uint8_t clk, bool *Q5){

	if ( DemodBufferLen < 64 ) return FALSE;
	uint8_t si = 0;
	for (uint8_t idx = 28; idx < 64; idx++){
		si = idx;
		if ( PackBits(si, 28, DemodBuffer) == 0x00 ) continue;

		uint8_t safer    = PackBits(si, 4, DemodBuffer); si += 4;     //master key
		uint8_t resv     = PackBits(si, 4, DemodBuffer); si += 4;     //was 7 & +=7+3 //should be only 4 bits if extended mode
		// 2nibble must be zeroed.
		// moved test to here, since this gets most faults first.
		if ( resv > 0x00) continue;

		uint8_t xtRate   = PackBits(si, 3, DemodBuffer); si += 3;     //extended mode part of rate
		int bitRate      = PackBits(si, 3, DemodBuffer); si += 3;     //bit rate
		if (bitRate > 7) continue;
		uint8_t extend   = PackBits(si, 1, DemodBuffer); si += 1;     //bit 15 extended mode
		uint8_t modread  = PackBits(si, 5, DemodBuffer); si += 5+2+1; 
		//uint8_t pskcr   = PackBits(si, 2, DemodBuffer); si += 2+1;  //could check psk cr
		uint8_t nml01    = PackBits(si, 1, DemodBuffer); si += 1+5;   //bit 24, 30, 31 could be tested for 0 if not extended mode
		uint8_t nml02    = PackBits(si, 2, DemodBuffer); si += 2;
		
		//if extended mode
		bool extMode =( (safer == 0x6 || safer == 0x9) && extend) ? TRUE : FALSE;

		if (!extMode){
			if (nml01 || nml02 || xtRate) continue;
		}
		//test modulation
		if (!testModulation(mode, modread)) continue;
		if (!testBitRate(bitRate, clk)) continue;
		*fndBitRate = bitRate;
		*offset = idx;
		*Q5 = FALSE;
		return TRUE;
	}
	if (testQ5(mode, offset, fndBitRate, clk)) {
		*Q5 = TRUE;
		return TRUE;
	}
	return FALSE;
}

void printT55xxBlock(const char *blockNum){
	
	uint8_t i = config.offset;
	uint8_t endpos = 32 + i;
	uint32_t blockData = 0;
	uint8_t bits[64] = {0x00};

	if ( !DemodBufferLen) return;

	if ( endpos > DemodBufferLen){
		PrintAndLog("The configured offset %d is too big. Possible offset: %d)", i, DemodBufferLen-32);
		return;
	}

	for (; i < endpos; ++i)
		bits[i - config.offset]=DemodBuffer[i];

	blockData = PackBits(0, 32, bits);

	PrintAndLog("  %s | %08X | %s", blockNum, blockData, sprint_bin(bits,32));
}

int special(const char *Cmd) {
	uint32_t blockData = 0;
	uint8_t bits[32] = {0x00};

	PrintAndLog("OFFSET | DATA       | BINARY");
	PrintAndLog("----------------------------------------------------");
	int i,j = 0;
	for (; j < 64; ++j){
		
		for (i = 0; i < 32; ++i)
			bits[i]=DemodBuffer[j+i];
	
		blockData = PackBits(0, 32, bits);
		
		PrintAndLog("    %02d | 0x%08X | %s",j , blockData, sprint_bin(bits,32));	
	}
	return 0;
}

int printConfiguration( t55xx_conf_block_t b){
	PrintAndLog("Chip Type  : %s", (b.Q5) ? "T5555(Q5)" : "T55x7");
	PrintAndLog("Modulation : %s", GetSelectedModulationStr(b.modulation) );
	PrintAndLog("Bit Rate   : %s", GetBitRateStr(b.bitrate) );
	PrintAndLog("Inverted   : %s", (b.inverted) ? "Yes" : "No" );
	PrintAndLog("Offset     : %d", b.offset);
	PrintAndLog("Block0     : 0x%08X", b.block0);
	PrintAndLog("");
	return 0;
}

int CmdT55xxWakeUp(const char *Cmd) {
	uint32_t password = 0;
	uint8_t cmdp = 0;
	bool errors = true;
	while(param_getchar(Cmd, cmdp) != 0x00) {
		switch(param_getchar(Cmd, cmdp)) {
		case 'h':
		case 'H':
			return usage_t55xx_wakup();
		case 'p':
		case 'P':
			password = param_get32ex(Cmd, cmdp+1, 0xFFFFFFFF, 16);
			cmdp += 2;
			errors = false;
			break;
		default:
			PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
			errors = true;
			break;
		}
	}
	if (errors) return usage_t55xx_wakup();

	UsbCommand c = {CMD_T55XX_WAKEUP, {password, 0, 0}};
	clearCommandBuffer();
	SendCommand(&c);
	PrintAndLog("Wake up command sent. Try read now");
	return 0;
}

int CmdT55xxWriteBlock(const char *Cmd) {
	uint8_t block = 0xFF; //default to invalid block
	uint32_t data = 0; //default to blank Block 
	uint32_t password = 0; //default to blank Block 7
	bool usepwd = false;
	bool page1 = false;	
	bool gotdata = false;
	bool errors = false;
	uint8_t cmdp = 0;
	while(param_getchar(Cmd, cmdp) != 0x00 && !errors) {
		switch(param_getchar(Cmd, cmdp)) {
		case 'h':
		case 'H':
			return usage_t55xx_write();
		case 'b':
		case 'B':
			errors |= param_getdec(Cmd, cmdp+1, &block);
			cmdp += 2;
			break;
		case 'd':
		case 'D':
			data = param_get32ex(Cmd, cmdp+1, 0, 16);
			gotdata	= true;
			cmdp += 2;
			break;
		case 'p':
		case 'P':
			password = param_get32ex(Cmd, cmdp+1, 0, 16);
			usepwd = true;
			cmdp += 2;
			break;
		case '1':
			page1 = true;
			cmdp++;
			break;
		default:
			PrintAndLog("Unknown parameter '%c'", param_getchar(Cmd, cmdp));
			errors = true;
			break;
		}
	}
	if (errors || !gotdata) return usage_t55xx_write();

	if (block > 7) {
		PrintAndLog("Block number must be between 0 and 7");
		return 0;
	}
	
	UsbCommand c = {CMD_T55XX_WRITE_BLOCK, {data, block, 0}};
	UsbCommand resp;
 	c.d.asBytes[0] = (page1) ? 0x2 : 0; 

	char pwdStr[16] = {0};
	snprintf(pwdStr, sizeof(pwdStr), "pwd: 0x%08X", password);

	PrintAndLog("Writing page %d  block: %02d  data: 0x%08X %s", page1, block, data,  (usepwd) ? pwdStr : "" );

	//Password mode
	if (usepwd) {
		c.arg[2] = password;
		c.d.asBytes[0] |= 0x1; 
	}
	clearCommandBuffer();
	SendCommand(&c);
	if (!WaitForResponseTimeout(CMD_ACK, &resp, 1000)){
		PrintAndLog("Error occurred, device did not ACK write operation. (May be due to old firmware)");
		return 0;
	}
	return 1;
}

int CmdT55xxReadTrace(const char *Cmd) {
	char cmdp = param_getchar(Cmd, 0);
	bool pwdmode = false;
	uint32_t password = 0;
	if (strlen(Cmd) > 1 || cmdp == 'h' || cmdp == 'H') 
		return usage_t55xx_trace();

	if (strlen(Cmd)==0)
		if ( !AquireData( T55x7_PAGE1, REGULAR_READ_MODE_BLOCK, pwdmode, password ) )
			return 0;
	
	if (!DecodeT55xxBlock()) return 0;

	if ( !DemodBufferLen) return 0;

	RepaintGraphWindow();
	uint8_t repeat = 0;
	if (config.offset > 5) 
		repeat = 32;
	uint8_t si = config.offset+repeat;
	uint32_t bl1     = PackBits(si, 32, DemodBuffer);
	uint32_t bl2     = PackBits(si+32, 32, DemodBuffer);
	
	uint32_t acl     = PackBits(si, 8,  DemodBuffer); si += 8;
	uint32_t mfc     = PackBits(si, 8,  DemodBuffer); si += 8;
	uint32_t cid     = PackBits(si, 5,  DemodBuffer); si += 5;
	uint32_t icr     = PackBits(si, 3,  DemodBuffer); si += 3;
	uint32_t year    = PackBits(si, 4,  DemodBuffer); si += 4;
	uint32_t quarter = PackBits(si, 2,  DemodBuffer); si += 2;
	uint32_t lotid   = PackBits(si, 14, DemodBuffer); si += 14;
	uint32_t wafer   = PackBits(si, 5,  DemodBuffer); si += 5;
	uint32_t dw      = PackBits(si, 15, DemodBuffer); 
	
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	if ( year > tm.tm_year-110)
		year += 2000;
	else
		year += 2010;

	if (config.Q5) PrintAndLog("*** Warning *** Info read off a Q5 will not work as expected");
	if ( acl != 0xE0 ) {
		PrintAndLog("The modulation is most likely wrong since the ACL is not 0xE0. ");
		return 0;
	}
	PrintAndLog("");
	PrintAndLog("-- T55xx Trace Information ----------------------------------");
	PrintAndLog("-------------------------------------------------------------");
	PrintAndLog(" ACL Allocation class (ISO/IEC 15963-1)  : 0x%02X (%d)", acl, acl);
	PrintAndLog(" MFC Manufacturer ID (ISO/IEC 7816-6)    : 0x%02X (%d) - %s", mfc, mfc, getTagInfo(mfc));
	PrintAndLog(" CID                                     : 0x%02X (%d) - %s", cid, cid, GetModelStrFromCID(cid));
	PrintAndLog(" ICR IC Revision                         : %d",icr );
	PrintAndLog(" Manufactured");
	PrintAndLog("     Year/Quarter : %d/%d",year, quarter);
	PrintAndLog("     Lot ID       : %d", lotid );
	PrintAndLog("     Wafer number : %d", wafer);
	PrintAndLog("     Die Number   : %d", dw);
	PrintAndLog("-------------------------------------------------------------");
	PrintAndLog(" Raw Data - Page 1");
	PrintAndLog("     Block 1  : 0x%08X  %s", bl1, sprint_bin(DemodBuffer+config.offset+repeat,32) );
	PrintAndLog("     Block 2  : 0x%08X  %s", bl2, sprint_bin(DemodBuffer+config.offset+repeat+32,32) );
	PrintAndLog("-------------------------------------------------------------");

	/*
	TRACE - BLOCK O
		Bits	Definition								HEX
		1-8		ACL Allocation class (ISO/IEC 15963-1)	0xE0 
		9-16	MFC Manufacturer ID (ISO/IEC 7816-6)	0x15 Atmel Corporation
		17-21	CID										0x1 = Atmel ATA5577M1  0x2 = Atmel ATA5577M2 
		22-24	ICR IC revision
		25-28	YEAR (BCD encoded) 						9 (= 2009)
		29-30	QUARTER									1,2,3,4 
		31-32	LOT ID
	
	TRACE - BLOCK 1
		1-12	LOT ID  
		13-17	Wafer number
		18-32	DW,  die number sequential
	*/
	
  return 0;
}

int CmdT55xxInfo(const char *Cmd){
	/*
		Page 0 Block 0 Configuration data.
		Normal mode
		Extended mode
	*/
	bool pwdmode = false;
	uint32_t password = 0;
	char cmdp = param_getchar(Cmd, 0);

	if (strlen(Cmd) > 1 || cmdp == 'h' || cmdp == 'H')
		return usage_t55xx_info();
	
	if (strlen(Cmd)==0)
		if ( !AquireData( T55x7_PAGE0, T55x7_CONFIGURATION_BLOCK, pwdmode, password ) )
			return 1;

	if (!DecodeT55xxBlock()) return 1;

	if ( DemodBufferLen < 32) return 1;

	uint8_t si = config.offset;
	uint32_t bl0      = PackBits(si, 32, DemodBuffer);
	
	uint32_t safer    = PackBits(si, 4, DemodBuffer); si += 4;	
	uint32_t resv     = PackBits(si, 7, DemodBuffer); si += 7;
	uint32_t dbr      = PackBits(si, 3, DemodBuffer); si += 3;
	uint32_t extend   = PackBits(si, 1, DemodBuffer); si += 1;
	uint32_t datamod  = PackBits(si, 5, DemodBuffer); si += 5;
	uint32_t pskcf    = PackBits(si, 2, DemodBuffer); si += 2;
	uint32_t aor      = PackBits(si, 1, DemodBuffer); si += 1;	
	uint32_t otp      = PackBits(si, 1, DemodBuffer); si += 1;	
	uint32_t maxblk   = PackBits(si, 3, DemodBuffer); si += 3;
	uint32_t pwd      = PackBits(si, 1, DemodBuffer); si += 1;	
	uint32_t sst      = PackBits(si, 1, DemodBuffer); si += 1;	
	uint32_t fw       = PackBits(si, 1, DemodBuffer); si += 1;
	uint32_t inv      = PackBits(si, 1, DemodBuffer); si += 1;	
	uint32_t por      = PackBits(si, 1, DemodBuffer); si += 1;
	if (config.Q5) PrintAndLog("*** Warning *** Config Info read off a Q5 will not display as expected");
	PrintAndLog("");
	PrintAndLog("-- T55xx Configuration & Tag Information --------------------");
	PrintAndLog("-------------------------------------------------------------");
	PrintAndLog(" Safer key                 : %s", GetSaferStr(safer));
	PrintAndLog(" reserved                  : %d", resv);
	PrintAndLog(" Data bit rate             : %s", GetBitRateStr(dbr));
	PrintAndLog(" eXtended mode             : %s", (extend) ? "Yes - Warning":"No");
	PrintAndLog(" Modulation                : %s", GetModulationStr(datamod));
	PrintAndLog(" PSK clock frequency       : %d", pskcf);
	PrintAndLog(" AOR - Answer on Request   : %s", (aor) ? "Yes":"No");
	PrintAndLog(" OTP - One Time Pad        : %s", (otp) ? "Yes - Warning":"No" );
	PrintAndLog(" Max block                 : %d", maxblk);
	PrintAndLog(" Password mode             : %s", (pwd) ? "Yes":"No");
	PrintAndLog(" Sequence Start Terminator : %s", (sst) ? "Yes":"No");
	PrintAndLog(" Fast Write                : %s", (fw)  ? "Yes":"No");
	PrintAndLog(" Inverse data              : %s", (inv) ? "Yes":"No");
	PrintAndLog(" POR-Delay                 : %s", (por) ? "Yes":"No");
	PrintAndLog("-------------------------------------------------------------");
	PrintAndLog(" Raw Data - Page 0");
	PrintAndLog("     Block 0  : 0x%08X  %s", bl0, sprint_bin(DemodBuffer+config.offset,32) );
	PrintAndLog("-------------------------------------------------------------");
	
	return 0;
}

int CmdT55xxDump(const char *Cmd){

	uint32_t password = 0;
	char cmdp = param_getchar(Cmd, 0);
	bool override = false;
	if ( cmdp == 'h' || cmdp == 'H') return usage_t55xx_dump();

	bool usepwd = ( strlen(Cmd) > 0);	
	if ( usepwd ){
		password = param_get32ex(Cmd, 0, 0, 16);
		if (param_getchar(Cmd, 1) =='o' )
			override = true;
	}
	
	printT5xxHeader(0);
	for ( uint8_t i = 0; i <8; ++i)
		T55xxReadBlock(i, 0, usepwd, override, password);

	printT5xxHeader(1);
	for ( uint8_t	i = 0; i<4; i++)
		T55xxReadBlock(i, 1, usepwd, override, password);		

	return 1;
}

int AquireData( uint8_t page, uint8_t block, bool pwdmode, uint32_t password ){
	// arg0 bitmodes:
	// bit0 = pwdmode
	// bit1 = page to read from
	uint8_t arg0 = (page<<1) | pwdmode;
	UsbCommand c = {CMD_T55XX_READ_BLOCK, {arg0, block, password}};

	clearCommandBuffer();
	SendCommand(&c);
	if ( !WaitForResponseTimeout(CMD_ACK,NULL,2500) ) {
		PrintAndLog("command execution time out");
		return 0;
	}

	uint8_t got[12000];
	GetFromBigBuf(got,sizeof(got),0);
	WaitForResponse(CMD_ACK,NULL);
	setGraphBuf(got, sizeof(got));
	return 1;
}

char * GetBitRateStr(uint32_t id){
 	static char buf[25];

	char *retStr = buf;
		switch (id){
		case 0: 
			snprintf(retStr,sizeof(buf),"%d - RF/8",id);
			break;
		case 1:
			snprintf(retStr,sizeof(buf),"%d - RF/16",id);
			break;
		case 2:		
			snprintf(retStr,sizeof(buf),"%d - RF/32",id);
			break;
		case 3:
			snprintf(retStr,sizeof(buf),"%d - RF/40",id);
			break;
		case 4:
			snprintf(retStr,sizeof(buf),"%d - RF/50",id);
			break;
		case 5:
			snprintf(retStr,sizeof(buf),"%d - RF/64",id);
			break;
		case 6:
			snprintf(retStr,sizeof(buf),"%d - RF/100",id);
			break;
		case 7:
			snprintf(retStr,sizeof(buf),"%d - RF/128",id);
			break;
		default:
			snprintf(retStr,sizeof(buf),"%d - (Unknown)",id);
			break;
		}

	return buf;
}

char * GetSaferStr(uint32_t id){
	static char buf[40];
	char *retStr = buf;
	
	snprintf(retStr,sizeof(buf),"%d",id);
	if (id == 6) {
		snprintf(retStr,sizeof(buf),"%d - passwd",id);
	}
	if (id == 9 ){
		snprintf(retStr,sizeof(buf),"%d - testmode",id);
	}
	
	return buf;
}

char * GetModulationStr( uint32_t id){
	static char buf[60];
	char *retStr = buf;
	
	switch (id){
		case 0: 
			snprintf(retStr,sizeof(buf),"%d - DIRECT (ASK/NRZ)",id);
			break;
		case 1:
			snprintf(retStr,sizeof(buf),"%d - PSK 1 phase change when input changes",id);
			break;
		case 2:		
			snprintf(retStr,sizeof(buf),"%d - PSK 2 phase change on bitclk if input high",id);
			break;
		case 3:
			snprintf(retStr,sizeof(buf),"%d - PSK 3 phase change on rising edge of input",id);
			break;
		case 4:
			snprintf(retStr,sizeof(buf),"%d - FSK 1 RF/8  RF/5",id);
			break;
		case 5:
			snprintf(retStr,sizeof(buf),"%d - FSK 2 RF/8  RF/10",id);
			break;
		case 6:
			snprintf(retStr,sizeof(buf),"%d - FSK 1a RF/5  RF/8",id);
			break;
		case 7:
			snprintf(retStr,sizeof(buf),"%d - FSK 2a RF/10  RF/8",id);
			break;
		case 8:
			snprintf(retStr,sizeof(buf),"%d - Manchester",id);
			break;
		case 16:
			snprintf(retStr,sizeof(buf),"%d - Biphase",id);
			break;
		case 0x18:
			snprintf(retStr,sizeof(buf),"%d - Biphase a - AKA Conditional Dephase Encoding(CDP)",id);
			break;
		case 17:
			snprintf(retStr,sizeof(buf),"%d - Reserved",id);
			break;
		default:
			snprintf(retStr,sizeof(buf),"0x%02X (Unknown)",id);
			break;
		}
	return buf;
}

char * GetModelStrFromCID(uint32_t cid){
		
	static char buf[10];
	char *retStr = buf;
	
	if (cid == 1) snprintf(retStr, sizeof(buf),"ATA5577M1");
	if (cid == 2) snprintf(retStr, sizeof(buf),"ATA5577M2");	
	return buf;
}

char * GetSelectedModulationStr( uint8_t id){

	static char buf[20];
	char *retStr = buf;

	switch (id){
		case DEMOD_FSK:
			snprintf(retStr,sizeof(buf),"FSK");
			break;
		case DEMOD_FSK1:
			snprintf(retStr,sizeof(buf),"FSK1");
			break;
		case DEMOD_FSK1a:
			snprintf(retStr,sizeof(buf),"FSK1a");
			break;
		case DEMOD_FSK2:
			snprintf(retStr,sizeof(buf),"FSK2");
			break;
		case DEMOD_FSK2a:
			snprintf(retStr,sizeof(buf),"FSK2a");
			break;
		case DEMOD_ASK:		
			snprintf(retStr,sizeof(buf),"ASK");
			break;
		case DEMOD_NRZ:
			snprintf(retStr,sizeof(buf),"DIRECT/NRZ");
			break;
		case DEMOD_PSK1:
			snprintf(retStr,sizeof(buf),"PSK1");
			break;
		case DEMOD_PSK2:
			snprintf(retStr,sizeof(buf),"PSK2");
			break;
		case DEMOD_PSK3:
			snprintf(retStr,sizeof(buf),"PSK3");
			break;
		case DEMOD_BI:
			snprintf(retStr,sizeof(buf),"BIPHASE");
			break;
		case DEMOD_BIa:
			snprintf(retStr,sizeof(buf),"BIPHASEa - (CDP)");
			break;
		default:
			snprintf(retStr,sizeof(buf),"(Unknown)");
			break;
		}
	return buf;
}

uint32_t PackBits(uint8_t start, uint8_t len, uint8_t* bits){
	
	int i = start;
	int j = len-1;

	if (len > 32) return 0;

	uint32_t tmp = 0;
	for (; j >= 0; --j, ++i)
		tmp	|= bits[i] << j;

	return tmp;
}

int CmdResetRead(const char *Cmd) {
	UsbCommand c = {CMD_T55XX_RESET_READ, {0,0,0}};

	clearCommandBuffer();
	SendCommand(&c);
	if ( !WaitForResponseTimeout(CMD_ACK,NULL,2500) ) {
		PrintAndLog("command execution time out");
		return 0;
	}

	uint8_t got[BIGBUF_SIZE-1];
	GetFromBigBuf(got,sizeof(got),0);
	WaitForResponse(CMD_ACK,NULL);
	setGraphBuf(got, sizeof(got));
	return 1;
}

int CmdT55xxWipe(const char *Cmd) {
	char writeData[20] = {0};
	char *ptrData = writeData;

	PrintAndLog("\nBeginning Wipe of a T55xx tag (assuming the tag is not password protected)\n");

	//try with the default password to reset block 0  (with a pwd should work even if pwd bit not set)
	snprintf(ptrData,sizeof(writeData),"b 0 d 00088040 p 0");

	if (!CmdT55xxWriteBlock(ptrData))
		PrintAndLog("Error writing blk 0");

	for (uint8_t blk = 1; blk<8; blk++) {
		snprintf(ptrData,sizeof(writeData),"b %d d 0", blk);
		if (!CmdT55xxWriteBlock(ptrData))
			PrintAndLog("Error writing blk %d", blk);

		memset(writeData, sizeof(writeData), 0x00);
	}
	return 0;
}

static command_t CommandTable[] = {
  {"help",     CmdHelp,           1, "This help"},
  {"config",   CmdT55xxSetConfig, 1, "Set/Get T55XX configuration (modulation, inverted, offset, rate)"},
  {"detect",   CmdT55xxDetect,    1, "[1] Try detecting the tag modulation from reading the configuration block."},
  {"read",     CmdT55xxReadBlock, 0, "b <block> p [password] [o] [1] -- Read T55xx block data. Optional [p password], [override], [page1]"},
  {"resetread",CmdResetRead,      0, "Send Reset Cmd then lf read the stream to attempt to identify the start of it (needs a demod and/or plot after)"},
  {"write",    CmdT55xxWriteBlock,0, "b <block> d <data> p [password] [1] -- Write T55xx block data. Optional [p password], [page1]"},
  {"trace",    CmdT55xxReadTrace, 1, "[1] Show T55x7 traceability data (page 1/ blk 0-1)"},
  {"info",     CmdT55xxInfo,      1, "[1] Show T55x7 configuration data (page 0/ blk 0)"},
  {"dump",     CmdT55xxDump,      0, "[password] [o] Dump T55xx card block 0-7. Optional [password], [override]"},
  {"special",  special,           0, "Show block changes with 64 different offsets"},
  {"wakeup",   CmdT55xxWakeUp,    0, "Send AOR wakeup command"},
  {"wipe",     CmdT55xxWipe,      0, "Wipe a T55xx tag and set defaults (will destroy any data on tag)"},
  {NULL, NULL, 0, NULL}
};

int CmdLFT55XX(const char *Cmd) {
  CmdsParse(CommandTable, Cmd);
  return 0;
}

int CmdHelp(const char *Cmd) {
  CmdsHelp(CommandTable);
  return 0;
}
