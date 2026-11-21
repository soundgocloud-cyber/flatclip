#include "PluginEditor.h"

using namespace flattop_ui;

namespace
{
// Sizes: THRESHOLD is the biggest, every other knob is the same size,
// and the EQ knobs are smaller and all equal.
constexpr int kBigKnob   = 160;
constexpr int kKnob      = 88;
constexpr int kSmallKnob = 52;
constexpr int kFaderH    = 110;
constexpr int kCaptionH  = 18;
constexpr int kTextH     = 18;
constexpr int kSmallTextH = 16;
} // namespace

FlatTopEditor::FlatTopEditor (FlatTopProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    initDial (input,     ids::input,     "INPUT",     0.0f,   kTeal);
    initDial (threshold, ids::threshold, "THRESHOLD", -6.0f,  kHot);
    initDial (post,      ids::post,      "POST",      0.0f,   kTeal);
    initDial (sat,       ids::sat,       "SAT",       0.20f,  kTeal);
    initDial (sheen,     ids::sheen,     "SHEEN",     0.0f,   kTeal);
    initDial (density,   ids::density,   "DENSITY",   0.25f,  kTeal);
    initDial (output,    ids::output,    "OUTPUT",    0.0f,   kTeal);

    initDial (driveLow,  ids::driveLow,  "LOW DRIVE",  0.0f,    kBlue);
    initDial (driveMid,  ids::driveMid,  "MID DRIVE",  0.0f,    kBlue);
    initDial (driveHigh, ids::driveHigh, "HIGH DRIVE", 0.0f,    kBlue);
    initDial (xoLow,     ids::xoLow,     "LOW / MID",  120.0f,  kBlue);
    initDial (xoHigh,    ids::xoHigh,    "MID / HIGH", 4000.0f, kBlue);

    initDial (eqLowG,  ids::eqLowGain,  "LOW",  0.0f, kAmber, Kind::fader);
    initDial (eqMidG,  ids::eqMidGain,  "MID",  0.0f, kAmber, Kind::fader);
    initDial (eqHighG, ids::eqHighGain, "HIGH", 0.0f, kAmber, Kind::fader);
    initDial (eqLowF,  ids::eqLowFreq,  "LOW FREQ",  100.0f,  kAmber, Kind::smallKnob);
    initDial (eqMidF,  ids::eqMidFreq,  "MID FREQ",  1000.0f, kAmber, Kind::smallKnob);
    initDial (eqHighF, ids::eqHighFreq, "HIGH FREQ", 8000.0f, kAmber, Kind::smallKnob);

    multiDials = { &driveLow, &driveMid, &driveHigh, &xoLow, &xoHigh };

    // threshold on/off: lights up green when ON, completely dark when OFF
    thresholdOn.setButtonText ("ON");
    thresholdOn.setClickingTogglesState (true);
    thresholdOn.setColour (juce::ToggleButton::tickColourId, kGreen);
    addAndMakeVisible (thresholdOn);
    thresholdOnAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::thresholdOn, thresholdOn);

    multi.setButtonText ("MULTI");
    multi.setClickingTogglesState (true);
    multi.setColour (juce::ToggleButton::tickColourId, kAmber);
    addAndMakeVisible (multi);
    multiAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, ids::multi, multi);

    setSize (920, 660);

    lastMulti = multi.getToggleState();
    for (auto* d : multiDials) d->slider.setEnabled (lastMulti);
    startTimerHz (15);
}

FlatTopEditor::~FlatTopEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FlatTopEditor::initDial (Dial& d, const char* paramId, const juce::String& caption, float defaultValue,
                              juce::Colour colour, Kind kind)
{
    d.caption = caption;
    auto& s = d.slider;

    if (kind == Kind::fader)
    {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, kTextH);
    }
    else
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, kind == Kind::smallKnob ? 64 : 76,
                           kind == Kind::smallKnob ? kSmallTextH : kTextH);
        s.setMouseDragSensitivity (200);
    }

    s.setColour (juce::Slider::rotarySliderFillColourId, colour);
    s.setColour (juce::Slider::thumbColourId, colour);
    addAndMakeVisible (s);

    d.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, s);

    // double-click anything -> snaps back to default (EQ faders: 0 dB)
    s.setDoubleClickReturnValue (true, (double) defaultValue);

    allDials.push_back (&d);
}

void FlatTopEditor::place (Dial& d, int centreX, int captionY, int knobSize)
{
    const bool isFader = d.slider.getSliderStyle() == juce::Slider::LinearVertical;
    const bool small   = (! isFader) && knobSize == kSmallKnob;
    const int textH    = small ? kSmallTextH : kTextH;
    const int w        = juce::jmax (knobSize, isFader ? 64 : (small ? 64 : 76));
    const int h        = (isFader ? kFaderH : knobSize) + textH;

    d.captionBounds = { centreX - 54, captionY, 108, kCaptionH };
    d.slider.setBounds (centreX - w / 2, captionY + kCaptionH, w, h);
}

void FlatTopEditor::resized()
{
    const int W = getWidth();
    multi.setBounds (W - 20 - 130, 8, 130, 44);

    chainPanel = { 20, 56, 880, 272 };
    multiPanel = { 20, 342, 560, 298 };
    eqPanel    = { 600, 342, 300, 298 };

    // ---- row 1: INPUT | THRESHOLD(big) | POST | SAT | SHEEN | DENSITY | OUTPUT
    const int cell = 108, bigCell = 232;
    const int bigTop = chainPanel.getY() + 19;               // button row starts here
    const int bigCaptionY = bigTop + 34 + 6;
    const int bigCentreY = bigCaptionY + kCaptionH + kBigKnob / 2;
    const int knobCaptionY = bigCentreY - kKnob / 2 - kCaptionH;

    int x = chainPanel.getX();
    place (input, x + cell / 2, knobCaptionY, kKnob);                   x += cell;
    const int bigCx = x + bigCell / 2;
    thresholdOn.setBounds (bigCx - 45, bigTop, 90, 34);
    place (threshold, bigCx, bigCaptionY, kBigKnob);                    x += bigCell;
    place (post,    x + cell / 2, knobCaptionY, kKnob);                 x += cell;
    place (sat,     x + cell / 2, knobCaptionY, kKnob);                 x += cell;
    place (sheen,   x + cell / 2, knobCaptionY, kKnob);                 x += cell;
    place (density, x + cell / 2, knobCaptionY, kKnob);                 x += cell;
    place (output,  x + cell / 2, knobCaptionY, kKnob);

    // ---- row 2 left: MULTI panel
    {
        const int capY = multiPanel.getY() + 76;
        int mx = multiPanel.getX() + 10;
        for (auto* d : multiDials) { place (*d, mx + cell / 2, capY, kKnob); mx += cell; }
    }

    // ---- row 2 right: 3-band master EQ (faders on top, small equal knobs underneath)
    {
        const int colW = 96;
        const int startX = eqPanel.getX() + (eqPanel.getWidth() - colW * 3) / 2;
        const int faderCapY = eqPanel.getY() + 42;
        const int knobCapY  = faderCapY + kCaptionH + kFaderH + kTextH + 8;
        Dial* faders[3] = { &eqLowG, &eqMidG, &eqHighG };
        Dial* knobs[3]  = { &eqLowF, &eqMidF, &eqHighF };
        for (int i = 0; i < 3; ++i)
        {
            const int cx = startX + colW * i + colW / 2;
            place (*faders[i], cx, faderCapY, kFaderH);
            place (*knobs[i],  cx, knobCapY,  kSmallKnob);
        }
    }
}

void FlatTopEditor::timerCallback()
{
    const bool m = multi.getToggleState();
    if (m != lastMulti)
    {
        lastMulti = m;
        for (auto* d : multiDials) d->slider.setEnabled (m);
        repaint();
    }
}

void FlatTopEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1d23), 0.0f, 0.0f,
                                             juce::Colour (0xff0d0f12), 0.0f, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (kText);
    g.setFont (30.0f);
    g.setFont (g.getCurrentFont().boldened());
    g.drawText ("FLATTOP", 24, 8, 220, 40, juce::Justification::centredLeft, false);
    g.setColour (kTextDim);
    g.setFont (12.0f);
    g.drawText ("soft clipper  |  hard ceiling @ 0 dBFS  |  not oversampled", 150, 22, 420, 20,
                juce::Justification::centredLeft, false);

    // panels
    for (const auto& r : { chainPanel, multiPanel, eqPanel })
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (kPanelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    auto title = [&] (const juce::Rectangle<int>& panel, const juce::String& text, juce::Colour c) {
        g.setColour (c);
        g.setFont (13.0f);
        g.setFont (g.getCurrentFont().boldened());
        g.drawText (text, panel.getX() + 16, panel.getY() + 12, panel.getWidth() - 32, 20,
                    juce::Justification::centredLeft, false);
    };
    title (multiPanel, "MULTI BAND", lastMulti ? kBlue : kTextDim);
    title (eqPanel, "3-BAND MASTER EQ", kAmber);

    g.setColour (kTextDim);
    g.setFont (12.0f);
    g.drawFittedText (lastMulti ? "The clipper is split into lows, mids and highs. Each band has its own drive into the threshold."
                                : "Flip MULTI on (top right) to split the clipper into lows / mids / highs, each with its own drive.",
                      multiPanel.getX() + 16, multiPanel.getBottom() - 66, multiPanel.getWidth() - 32, 44,
                      juce::Justification::topLeft, 2);

    // captions
    for (auto* d : allDials)
    {
        const bool on = d->slider.isEnabled();
        g.setColour (on ? kText : kTextDim.withAlpha (0.5f));
        g.setFont (d == &threshold ? 13.0f : 11.5f);
        if (d == &threshold) g.setFont (g.getCurrentFont().boldened());
        g.drawText (d->caption, d->captionBounds, juce::Justification::centred, false);
    }
}
