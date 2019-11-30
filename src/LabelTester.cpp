//***********************************************************************************************
//Mixer module for VCV Rack by Steve Baker and Marc Boulé 
//
//Based on code from the Fundamental plugin by Andrew Belt 
//See ./LICENSE.md for all licenses
//***********************************************************************************************


#include "MindMeldModular.hpp"


struct LabelTester : Module {
	
	enum ParamIds {
		NUM_PARAMS
	};
	
	enum InputIds {
		NUM_INPUTS
	};
	
	enum OutputIds {
		NUM_OUTPUTS
	};
	
	enum LightIds {
		NUM_LIGHTS
	};
	

	// Constants
	// none


	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	// none
	
	// No need to save, with reset
	// none

	// No need to save, no reset
	RefreshCounter refresh;	
	
	
	LabelTester() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		onReset();
		
		panelTheme = 0;
	}
  
	void onReset() override {
		resetNonJson(false);
	}
	void resetNonJson(bool recurseNonJson) {
	}


	void onRandomize() override {
	}

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
				
		return rootJ;
	}


	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		resetNonJson(true);	}


	void onSampleRateChange() override {
	}
	

	void process(const ProcessArgs &args) override {

	}// process()
};


//-----------------------------------------------------------------------------


struct LabelTesterWidget : ModuleWidget {
	time_t oldTime = 0;
	
	LabelTesterWidget(LabelTester *module) {
		setModule(module);

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/labeltester.svg")));
		

	}
	
	void step() override {
		if (module) {
			// update labels from message bus at 1Hz
			time_t currentTime = time(0);
			if (currentTime != oldTime) {
				oldTime = currentTime;
				std::vector<MixerPayload> *mixerMessages = mixerMessageBus.currentValues();
				for (MixerPayload pl : *mixerMessages) {
					INFO("id: master label = %s", std::string(&(pl.masterName[0]), 6).c_str());
					for (int trk = 0; trk < pl.numTracks; trk++) {
						INFO("id: track %i label = %s", trk, std::string(&(pl.trackNames[trk * 4]), 4).c_str());
					}
				}
			}
		}
		Widget::step();
	}
};


Model *modelLabelTester = createModel<LabelTester, LabelTesterWidget>("LabelTester");
