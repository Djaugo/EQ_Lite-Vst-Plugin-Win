/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ_LiteAudioProcessorEditor)
};
