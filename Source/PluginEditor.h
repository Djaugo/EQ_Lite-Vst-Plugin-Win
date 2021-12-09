/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"




//=============================================================================
// Adding a look&feel class that will allow editing the area inside the custom knob ~A
struct LookAndFeel : juce::LookAndFeel_V4
{
    virtual void drawRotarySlider(juce::Graphics&,
        int x, int y, int width, int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider&) override;
};


// Adding a custom knob class   ~A
struct MyEQKnob1 : juce::Slider
{
    MyEQKnob1(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) :
        juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
        juce::Slider::TextEntryBoxPosition::TextBoxBelow),
        aParam(&rap),
        suffix(unitSuffix)

    {
        setLookAndFeel(&lnf);
    }

    ~MyEQKnob1()
    {
        setLookAndFeel(nullptr);
    }

    // Adding a container for the min max values to be displayed below each knob    ~A
    struct LabelPosition
    {
        float position;
        juce::String label;
    };

    juce::Array<LabelPosition> labelsArray;

    void paint(juce::Graphics& g) override;
    juce::Rectangle<int> getSliderBounds() const;
    int getTextHeight() const { return 14; }
    juce::String getDisplayString() const;
private:
    LookAndFeel lnf;
    juce::RangedAudioParameter* aParam;
    juce::String suffix;
};

// Creating a response curve window as a separate component obj so it doesn't draw outside the bounds   ~A

struct ResponseCurveWindow : juce::Component,
    juce::AudioProcessorParameter::Listener,
    juce::Timer
{
    ResponseCurveWindow(EQ_LiteAudioProcessor&);
    ~ResponseCurveWindow();

    void parameterValueChanged(int parameterIndex, float newValue) override;

    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override {};

    void timerCallback() override;

    void paint(juce::Graphics& g) override;

    void resized() override;

private:
    EQ_LiteAudioProcessor& audioProcessor;

    // Creating an atomic flag to decide if the component needs repainting  ~A
    juce::Atomic<bool> parametersChanged{ false };

    MonoChain monoChain;

    // Creating a function that will update the response curve the first time
    // GUI is displayed     ~A
    void updateChain();

    juce::Image responseBackground;

    // A function to return the slightly shrunken response window render area   ~A
    juce::Rectangle<int> getRenderArea();

    // And a function that returns even smaller analysis area 
    // (for boundary lines visibility)  ~A
    juce::Rectangle<int> getAnalysisArea();

};

//==============================================================================
/**
*/
// Adding classes to inherit from and override their methods to enable
// refreshing the response curve        ~A
class EQ_LiteAudioProcessorEditor  : public juce::AudioProcessorEditor

{
public:
    EQ_LiteAudioProcessorEditor (EQ_LiteAudioProcessor&);
    ~EQ_LiteAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;



private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    EQ_LiteAudioProcessor& audioProcessor;

  

    // Creating custom knob objects and a helper function that returns a vector of them    ~A

    MyEQKnob1 band1FreqKnob, band2FreqKnob, band3FreqKnob, band1QKnob, band2QKnob, band3QKnob, band1GainKnob, band2GainKnob,
              band3GainKnob, lowCutFreqKnob, highCutFreqKnob, lowCutSlopeKnob, highCutSlopeKnob;

    ResponseCurveWindow responseCurveWindow;

    //Creating an alias for the long path and declaring attachment objects to link GUI knobs with the audio processor   ~A
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment band1FreqKnobAttachment, band2FreqKnobAttachment, band3FreqKnobAttachment, band1QKnobAttachment, band2QKnobAttachment,
               band3QKnobAttachment, band1GainKnobAttachment, band2GainKnobAttachment,band3GainKnobAttachment, lowCutFreqKnobAttachment,
               highCutFreqKnobAttachment, lowCutSlopeKnobAttachment, highCutSlopeKnobAttachment;

    std::vector<juce::Component*> getComponents();

  

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ_LiteAudioProcessorEditor)
};
