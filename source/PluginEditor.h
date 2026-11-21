#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

class FlatTopEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FlatTopEditor (FlatTopProcessor&);
    ~FlatTopEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    // one control = slider + caption + parameter attachment
    struct Dial
    {
        juce::Slider slider;
        juce::String caption;
        juce::Rectangle<int> captionBounds;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment; // destroyed before slider
    };

    enum class Kind { knob, smallKnob, fader };
    void initDial (Dial&, const char* paramId, const juce::String& caption, float defaultValue,
                   juce::Colour colour, Kind kind = Kind::knob);
    void place (Dial&, int centreX, int captionY, int knobSize);

    FlatTopProcessor& proc;
    flattop_ui::FlatTopLookAndFeel lnf;

    // main chain
    Dial input, threshold, post, sat, sheen, density, output;
    // multi band
    Dial driveLow, driveMid, driveHigh, xoLow, xoHigh;
    // 3-band master EQ: faders + small freq knobs underneath
    Dial eqLowG, eqMidG, eqHighG, eqLowF, eqMidF, eqHighF;

    juce::ToggleButton thresholdOn, multi;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> thresholdOnAttachment, multiAttachment;

    std::vector<Dial*> allDials, multiDials;
    juce::Rectangle<int> chainPanel, multiPanel, eqPanel;
    bool lastMulti = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlatTopEditor)
};
