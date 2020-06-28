//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************

#pragma once

#include "MindMeldModular.hpp"
#include "dsp/FirstOrderFilter.hpp"
#include "dsp/ButterworthFilters.hpp"



//*****************************************************************************
// Constants


// Communications between mixer and auxspander
template <int N_TRK, int N_GRP>
struct ExpansionInterface {
	enum AuxFromMotherIds { // for expander messages from main to aux panel
		// Fast (sample-rate)
		AFM_UPDATE_SLOW,
		ENUMS(AFM_AUX_SENDS, (N_TRK + N_GRP) * 2), // Trk1L, Trk1R, Trk2L, Trk2R ... Trk16L, Trk16R, Grp1L, Grp1R ... Grp4L, Grp4R
		AFM_VU_INDEX, // a return VU related value; index 0-3 : quad vu floats of a given aux
		ENUMS(AFM_VU_VALUES, 4),
		
		// Slow (sample-rate / 256)
		ENUMS(AFM_TRACK_GROUP_NAMES, N_TRK + N_GRP), 
		AFM_COLOR_AND_CLOAK,
		AFM_DIRECT_AND_PAN_MODES,
		AFM_TRACK_MOVE,
		AFM_TRK_GRP_RESET,
		AFM_AUXSENDMUTE_GROUPED_RETURN,
		ENUMS(AFM_TRK_DISP_COL, N_TRK / 4 + 1), // 4 tracks per dword, 4 (2) groups in last dword
		AFM_ECO_MODE,
		ENUMS(AFM_FADE_GAINS, 4),
		AFM_MOMENTARY_CVBUTTONS,
		AFM_LINEARVOLCVINPUTS,
		ENUMS(AFM_MUTE_GHOST, 4), // mute ghost of each aux
		AFM_NUM_VALUES
	};
		
	enum MotherFromAuxIds { // for expander messages from aux panel to main
		// Fast (sample-rate)	
		MFA_UPDATE_SLOW,
		ENUMS(MFA_AUX_RETURNS, 8), // left A, B, C, D, right A, B, C, D
		ENUMS(MFA_AUX_RET_FADER, 12),// MFA_AUX_RET_FADER (4 floats), MFA_AUX_RET_PAN (4 floats), MFA_AUX_RET_FADER_CV (4 floats)
		
		// Slow (sample-rate / 256)
		MFA_AUX_DIR_OUTS,// direct outs modes for all four aux
		MFA_AUX_STEREO_PANS,// stereo pan modes for all four aux
		ENUMS(MFA_AUX_NAMES, 4),
		MFA_AUX_VUCOL,
		MFA_AUX_DISPCOL,
		ENUMS(MFA_AUX_VALUES20, 20),// MFA_AUX_MUTE_SOLO_GROUP (12 floats) and MFA_AUX_FADE_RATE_AND_PROFILE (8 floats)
		MFA_NUM_VALUES
	};	
};

struct MfaExpInterface {// for messages to mother from expander
	// Fast (sample-rate)	
	bool updateSlow = false;
	float auxReturns[8] = {0.0f};
	float auxRetFaderPanFadercv[12] = {0.0f};
	
	// Slow (sample-rate / 256), no need to init
	PackedBytes4 directOutsModeLocalAux;
	PackedBytes4 stereoPanModeLocalAux;
	alignas(4) char auxLabels[4 * 4 + 4];
	PackedBytes4 auxVuColors;
	PackedBytes4 auxDispColors;
	float values20[20];
};


// Global
struct GlobalConst {
	static const int 		masterFaderScalingExponent = 3; // for example, 3 is x^3 scaling
	static constexpr float 	masterFaderMaxLinearGain = 2.0f; // for example, 2.0f is +6 dB
	static const int 		trkAndGrpFaderScalingExponent = 3; 
	static constexpr float 	trkAndGrpFaderMaxLinearGain = 2.0f; 
	static const int 		globalAuxReturnScalingExponent = 3; 
	static constexpr float 	globalAuxReturnMaxLinearGain = 2.0f; 
	static const int 		individualAuxSendScalingExponent = 2;
	static constexpr float 	individualAuxSendMaxLinearGain = 1.0f;
	static const int 		globalAuxSendScalingExponent = 2; 
	static constexpr float 	globalAuxSendMaxLinearGain = 4.0f; 
	
	static constexpr float antipopSlewFast = 125.0f;// for pan/fader when linear, and mute/solo
	static constexpr float antipopSlewSlow = 25.0f;// for pan/fader when not linear
	static constexpr float minFadeRate = 0.1f;
	static constexpr float minHPFCutoffFreq = 20.0f;
	static constexpr float defHPFCutoffFreq = 13.0f;
	static constexpr float maxLPFCutoffFreq = 20000.0f;
	static constexpr float defLPFCutoffFreq = 20010.0f;
};




//*****************************************************************************
// Math

// Calculate std::sin(theta) using MacLaurin series
// Calculate std::cos(theta) using cos(x) = sin(Pi/2 - x)
// Assumes: 0 <= theta <= Pi/2
static inline void sinCos(float *destSin, float *destCos, float theta) {
	*destSin = theta + std::pow(theta, 3) * (-0.166666667f + theta * theta * 0.00833333333f);
	theta = float(M_PI_2) - theta;
	*destCos = theta + std::pow(theta, 3) * (-0.166666667f + theta * theta * 0.00833333333f);
}
static inline void sinCosSqrt2(float *destSin, float *destCos, float theta) {
	sinCos(destSin, destCos, theta);
	*destSin *= float(M_SQRT2);
	*destCos *= float(M_SQRT2);
}


static inline float calcDimGainIntegerDB(float dimGain) {
	float integerDB = std::round(20.0f * std::log10(dimGain));
	return std::pow(10.0f, integerDB / 20.0f);
}



//*****************************************************************************
// Utility

float updateFadeGain(float fadeGain, float target, float *fadeGainX, float *fadeGainXr, float timeStepX, float shape, bool symmetricalFade);

struct TrackSettingsCpBuffer {
	// first level of copy paste (copy copy-paste of track settings)
	float gainAdjust;
	float fadeRate;
	float fadeProfile;
	float hpfCutoffFreq;// !! user must call filters' setCutoffs manually when copy pasting these
	float lpfCutoffFreq;// !! user must call filters' setCutoffs manually when copy pasting these
	int8_t directOutsMode;
	int8_t auxSendsMode;
	int8_t panLawStereo;
	int8_t vuColorThemeLocal;
	int8_t filterPos;
	int8_t dispColorLocal;
	int8_t polyStereo;
	float panCvLevel;
	float stereoWidth;
	int8_t invertInput;
	bool linkedFader;

	// second level of copy paste (for track re-ordering)
	float paGroup;
	float paFade;
	float paMute;
	float paSolo;
	float paPan;
	char trackName[4];// track names are not null terminated in MixerTracks
	float fadeGain;
	float target;
	float fadeGainX;
	float fadeGainXr;
	float fadeGainScaled;
};


static inline bool isLinked(unsigned long *linkBitMaskSrc, int index) {return (*linkBitMaskSrc & (1 << index)) != 0;}
static inline void toggleLinked(unsigned long *linkBitMaskSrc, int index) {*linkBitMaskSrc ^= (1 << index);}


//*****************************************************************************


enum ccIds {
	cloakedMode, // turn off track VUs only, keep master VUs (also called "Cloaked mode"), this has only two values, 0x0 and 0xFF so that it can be used in bit mask operations
	vuColorGlobal, // 0 is green, 1 is aqua, 2 is cyan, 3 is blue, 4 is purple, 5 is individual colors for each track/group/master (every user of vuColor must first test for != 5 before using as index into color table, or else array overflow)
	dispColorGlobal, // 0 is yellow, 1 is light-gray, 2 is green, 3 is aqua, 4 is cyan, 5 is blue, 6 is purple, 7 is per track
	detailsShow // bit 0 is knob param arc, bit 1 is knob cv arc, bit 2 is fader cv pointer
};
