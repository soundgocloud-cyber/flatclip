#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> freqRange (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi, 1.0f);
    r.setSkewForCentre (centre);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout FlatTopProcessor::createLayout()
{
    using namespace juce;
    using PID = ParameterID;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dbAttr = AudioParameterFloatAttributes()
                      .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })
                      .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); });

    auto pctAttr = AudioParameterFloatAttributes()
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                       .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue() / 100.0f; });

    auto hzAttr = AudioParameterFloatAttributes()
                      .withStringFromValueFunction ([] (float v, int) {
                          return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; })
                      .withValueFromStringFunction ([] (const String& t) {
                          const float v = t.getFloatValue();
                          return t.containsIgnoreCase ("k") ? v * 1000.0f : v; });

    auto addDb = [&] (const char* id, const char* name, float lo, float hi, float def) {
        p.push_back (std::make_unique<AudioParameterFloat> (PID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.01f), def, dbAttr));
    };
    auto addPct = [&] (const char* id, const char* name, float def) {
        p.push_back (std::make_unique<AudioParameterFloat> (PID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def, pctAttr));
    };
    auto addHz = [&] (const char* id, const char* name, float lo, float hi, float centre, float def) {
        p.push_back (std::make_unique<AudioParameterFloat> (PID { id, 1 }, name, freqRange (lo, hi, centre), def, hzAttr));
    };
    auto addBool = [&] (const char* id, const char* name, bool def) {
        p.push_back (std::make_unique<AudioParameterBool> (PID { id, 1 }, name, def));
    };

    // main chain
    addDb  (ids::input,     "Input",            -24.0f, 24.0f,   0.0f);
    addDb  (ids::threshold, "Threshold",        -24.0f,  0.0f,  -6.0f);
    addBool(ids::thresholdOn, "Threshold On",   true);
    addDb  (ids::post,      "Post",             -12.0f, 12.0f,   0.0f);
    addPct (ids::sat,       "Sat",                              0.20f);
    addPct (ids::sheen,     "Sheen",                            0.0f);
    addPct (ids::density,   "Density",                          0.25f);
    addDb  (ids::output,    "Output",           -24.0f,  0.0f,   0.0f);

    // multi band
    addBool(ids::multi,     "Multi",            false);
    addDb  (ids::driveLow,  "Low Drive",          0.0f, 24.0f,   0.0f);
    addDb  (ids::driveMid,  "Mid Drive",          0.0f, 24.0f,   0.0f);
    addDb  (ids::driveHigh, "High Drive",         0.0f, 24.0f,   0.0f);
    addHz  (ids::xoLow,     "Low Crossover",     40.0f,   400.0f,  120.0f,  120.0f);
    addHz  (ids::xoHigh,    "High Crossover",  1000.0f, 12000.0f, 4000.0f, 4000.0f);

    // 3-band master EQ
    addDb  (ids::eqLowGain,  "EQ Low",          -12.0f, 12.0f, 0.0f);
    addDb  (ids::eqMidGain,  "EQ Mid",          -12.0f, 12.0f, 0.0f);
    addDb  (ids::eqHighGain, "EQ High",         -12.0f, 12.0f, 0.0f);
    addHz  (ids::eqLowFreq,  "EQ Low Freq",      40.0f,   400.0f,   120.0f,  100.0f);
    addHz  (ids::eqMidFreq,  "EQ Mid Freq",     200.0f,  5000.0f,  1000.0f, 1000.0f);
    addHz  (ids::eqHighFreq, "EQ High Freq",   2000.0f, 16000.0f,  6000.0f, 8000.0f);

    return { p.begin(), p.end() };
}

FlatTopProcessor::FlatTopProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FLATTOP_STATE", createLayout())
{
}

void FlatTopProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    snapNext = true;
}

bool FlatTopProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void FlatTopProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n      = buffer.getNumSamples();

    for (int c = numIn; c < numOut; ++c)
        buffer.clear (c, 0, n);

    flattop::Params p;
    p.inputDb      = raw (ids::input);
    p.thresholdDb  = raw (ids::threshold);
    p.thresholdOn  = raw (ids::thresholdOn) > 0.5f;
    p.postDb       = raw (ids::post);
    p.outputDb     = raw (ids::output);
    p.sat          = raw (ids::sat);
    p.sheen        = raw (ids::sheen);
    p.density      = raw (ids::density);
    p.multi        = raw (ids::multi) > 0.5f;
    p.driveLowDb   = raw (ids::driveLow);
    p.driveMidDb   = raw (ids::driveMid);
    p.driveHighDb  = raw (ids::driveHigh);
    p.xoLowHz      = raw (ids::xoLow);
    p.xoHighHz     = raw (ids::xoHigh);
    p.eqLowDb      = raw (ids::eqLowGain);
    p.eqMidDb      = raw (ids::eqMidGain);
    p.eqHighDb     = raw (ids::eqHighGain);
    p.eqLowHz      = raw (ids::eqLowFreq);
    p.eqMidHz      = raw (ids::eqMidFreq);
    p.eqHighHz     = raw (ids::eqHighFreq);

    engine.setParams (p, snapNext);
    snapNext = false;
    engine.process (buffer.getArrayOfWritePointers(), juce::jmin (numIn, buffer.getNumChannels(), 2), n);
}

juce::AudioProcessorEditor* FlatTopProcessor::createEditor()
{
    return new FlatTopEditor (*this);
}

void FlatTopProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FlatTopProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlatTopProcessor();
}
