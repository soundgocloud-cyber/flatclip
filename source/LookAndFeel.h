#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace flattop_ui
{
inline const juce::Colour kPanel     { 0xff1c1f26 };
inline const juce::Colour kPanelEdge { 0xff2c313b };
inline const juce::Colour kText      { 0xffd9dde5 };
inline const juce::Colour kTextDim   { 0xff7d8594 };
inline const juce::Colour kGreen     { 0xff3dff7e };
inline const juce::Colour kAmber     { 0xffffb938 };
inline const juce::Colour kTeal      { 0xff39d0c6 };
inline const juce::Colour kHot       { 0xffff5b3a };
inline const juce::Colour kBlue      { 0xff5aa2ff };

class FlatTopLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FlatTopLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, kText);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, kAmber.withAlpha (0.35f));
        setColour (juce::Label::textColourId, kText);
        setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0f1115));
        setColour (juce::TextEditor::textColourId, kText);
        setColour (juce::TextEditor::outlineColourId, kPanelEdge);
        setColour (juce::TextEditor::focusedOutlineColourId, kAmber);
        setColour (juce::CaretComponent::caretColourId, kAmber);
    }

    // ----------------------------------------------------------------- knobs
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const float alpha  = slider.isEnabled() ? 1.0f : 0.35f;
        const auto accent  = slider.findColour (juce::Slider::rotarySliderFillColourId).withMultipliedAlpha (alpha);
        const float dia    = (float) juce::jmin (width, height);
        const auto centre  = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).getCentre();
        const float radius = dia * 0.5f - 3.0f;
        const float body   = radius * 0.80f;
        const float angle  = startAngle + sliderPos * (endAngle - startAngle);
        const float trackW = juce::jmax (3.0f, dia * 0.07f);

        // soft shadow + body
        g.setColour (juce::Colours::black.withAlpha (0.35f * alpha));
        g.fillEllipse (centre.x - body, centre.y - body + 3.0f, body * 2.0f, body * 2.0f);

        juce::ColourGradient grad (juce::Colour (0xff434954), centre.x, centre.y - body,
                                   juce::Colour (0xff1a1d23), centre.x, centre.y + body, false);
        grad.multiplyOpacity (alpha);
        g.setGradientFill (grad);
        g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);

        g.setColour (juce::Colour (0xff555c69).withAlpha (alpha));
        g.drawEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 1.2f);

        // track + value arc
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2a2e36).withAlpha (alpha));
        g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (angle > startAngle + 0.001f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
            g.setColour (accent);
            g.strokePath (value, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // pointer
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.8f, -body * 0.92f, 3.6f, body * 0.50f, 1.8f);
        g.setColour (juce::Colours::white.withAlpha (0.92f * alpha));
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    // ---------------------------------------------------------------- faders
    int getSliderThumbRadius (juce::Slider& slider) override
    {
        if (slider.getSliderStyle() == juce::Slider::LinearVertical)
            return 9; // keeps the 16 px thumb fully inside the component at both ends
        return LookAndFeel_V4::getSliderThumbRadius (slider);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const float alpha  = slider.isEnabled() ? 1.0f : 0.35f;
        const auto accent  = slider.findColour (juce::Slider::thumbColourId).withMultipliedAlpha (alpha);
        const float cx     = (float) x + (float) width * 0.5f;
        const float top    = juce::jmin (minSliderPos, maxSliderPos);
        const float bottom = juce::jmax (minSliderPos, maxSliderPos);
        const float zeroY  = juce::jlimit (top, bottom, (float) slider.getPositionOfValue (0.0));

        // groove
        g.setColour (juce::Colour (0xff0d0f13).withAlpha (alpha));
        g.fillRoundedRectangle (cx - 3.5f, top - 4.0f, 7.0f, (bottom - top) + 8.0f, 3.5f);
        g.setColour (juce::Colour (0xff2c313b).withAlpha (alpha));
        g.drawRoundedRectangle (cx - 3.5f, top - 4.0f, 7.0f, (bottom - top) + 8.0f, 3.5f, 1.0f);

        // fill between 0 dB and the thumb
        g.setColour (accent.withAlpha (0.85f * alpha));
        g.fillRoundedRectangle (cx - 2.0f, juce::jmin (zeroY, sliderPos), 4.0f, std::abs (sliderPos - zeroY), 2.0f);

        // 0 dB tick
        g.setColour (kTextDim.withAlpha (alpha));
        g.drawLine (cx - 15.0f, zeroY, cx + 15.0f, zeroY, 1.0f);

        // thumb
        const juce::Rectangle<float> thumb (cx - 18.0f, sliderPos - 8.0f, 36.0f, 16.0f);
        juce::ColourGradient grad (juce::Colour (0xff59606d), 0.0f, thumb.getY(),
                                   juce::Colour (0xff272b33), 0.0f, thumb.getBottom(), false);
        grad.multiplyOpacity (alpha);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (thumb, 4.0f);
        g.setColour (juce::Colour (0xff6a7281).withAlpha (alpha));
        g.drawRoundedRectangle (thumb, 4.0f, 1.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (thumb.getX() + 5.0f, thumb.getCentreY() - 1.0f, thumb.getWidth() - 10.0f, 2.0f, 1.0f);
    }

    // --------------------------------------------------------------- buttons
    // Lit button: glows in its tick colour when ON, completely dark (no glow) when OFF.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool /*down*/) override
    {
        const auto b   = button.getLocalBounds().toFloat().reduced (7.0f);
        const bool on  = button.getToggleState();
        const auto lit = button.findColour (juce::ToggleButton::tickColourId);
        const float r  = 7.0f;

        if (on)
        {
            for (int i = 4; i >= 1; --i)
            {
                g.setColour (lit.withAlpha (0.08f * (float) i));
                g.fillRoundedRectangle (b.expanded ((float) (5 - i) * 1.4f), r + (float) (5 - i));
            }
            g.setColour (lit);
            g.fillRoundedRectangle (b, r);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawRoundedRectangle (b.reduced (0.5f), r, 1.0f);
            g.setColour (juce::Colour (0xff04210f));
        }
        else
        {
            g.setColour (highlighted ? juce::Colour (0xff2a2e37) : juce::Colour (0xff20232a));
            g.fillRoundedRectangle (b, r);
            g.setColour (juce::Colour (0xff383d48));
            g.drawRoundedRectangle (b.reduced (0.5f), r, 1.0f);
            g.setColour (kTextDim);
        }

        g.setFont (13.0f);
        g.setFont (g.getCurrentFont().boldened());
        g.drawText (button.getButtonText(), b.toNearestInt(), juce::Justification::centred, false);
    }
};
} // namespace flattop_ui
