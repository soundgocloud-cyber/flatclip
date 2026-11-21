#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "FlatTopDSP.h"

namespace ids
{
inline constexpr const char* input       = "input";
inline constexpr const char* threshold   = "threshold";
inline constexpr const char* thresholdOn = "threshold_on";
inline constexpr const char* post        = "post";
inline constexpr const char* sat         = "sat";
inline constexpr const char* sheen       = "sheen";
inline constexpr const char* density     = "density";
inline constexpr const char* output      = "output";
inline constexpr const char* multi       = "multi";
inline constexpr const char* driveLow    = "drive_low";
inline constexpr const char* driveMid    = "drive_mid";
inline constexpr const char* driveHigh   = "drive_high";
inline constexpr const char* xoLow       = "xo_low";
inline constexpr const char* xoHigh      = "xo_high";
inline constexpr const char* eqLowGain   = "eq_low_gain";
inline constexpr const char* eqMidGain   = "eq_mid_gain";
inline constexpr const char* eqHighGain  = "eq_high_gain";
inline constexpr const char* eqLowFreq   = "eq_low_freq";
inline constexpr const char* eqMidFreq   = "eq_mid_freq";
inline constexpr const char* eqHighFreq  = "eq_high_freq";
} // namespace ids

class FlatTopProcessor : public juce::AudioProcessor
{
public:
    FlatTopProcessor();
    ~FlatTopProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FlatTop"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    flattop::Engine engine;
    bool snapNext = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlatTopProcessor)
};
