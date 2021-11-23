/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// Adding a custom knob class   ~A
struct myEQKnob1 : juce::Slider
{
    myEQKnob1() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag,
        juce::Slider::TextEntryBoxPosition::NoTextBox)
    {

    }
};



//==============================================================================
/**
*/
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

    myEQKnob1 band1FreqKnob, band2FreqKnob, band3FreqKnob, band1QKnob, band2QKnob, band3QKnob, band1GainKnob, band2GainKnob,
              band3GainKnob, lowCutFreqKnob, highCutFreqKnob, lowCutSlopeKnob, highCutSlopeKnob;

    //Creating an alias for the long path and declaring attachment objects to link GUI knobs with the audio processor   ~A
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment band1FreqKnobAttachment, band2FreqKnobAttachment, band3FreqKnobAttachment, band1QKnobAttachment, band2QKnobAttachment,
               band3QKnobAttachment, band1GainKnobAttachment, band2GainKnobAttachment,band3GainKnobAttachment, lowCutFreqKnobAttachment,
               highCutFreqKnobAttachment, lowCutSlopeKnobAttachment, highCutSlopeKnobAttachment;

    std::vector<juce::Component*> getComponents();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ_LiteAudioProcessorEditor)
};
