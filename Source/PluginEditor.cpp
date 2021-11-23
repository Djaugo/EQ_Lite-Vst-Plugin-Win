/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EQ_LiteAudioProcessorEditor::EQ_LiteAudioProcessorEditor(EQ_LiteAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    band1FreqKnobAttachment(audioProcessor.apvts, "Band1 Freq", band1FreqKnob),
    band2FreqKnobAttachment(audioProcessor.apvts, "Band2 Freq", band2FreqKnob),
    band3FreqKnobAttachment(audioProcessor.apvts, "Band3 Freq", band3FreqKnob),
    band1GainKnobAttachment(audioProcessor.apvts, "Band1 Gain", band1GainKnob),
    band2GainKnobAttachment(audioProcessor.apvts, "Band2 Gain", band2GainKnob),
    band3GainKnobAttachment(audioProcessor.apvts, "Band3 Gain", band3GainKnob),
    band1QKnobAttachment(audioProcessor.apvts, "Band1 Quality", band1QKnob),
    band2QKnobAttachment(audioProcessor.apvts, "Band2 Quality", band2QKnob),
    band3QKnobAttachment(audioProcessor.apvts, "Band3 Quality", band3QKnob),
    lowCutFreqKnobAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqKnob),
    lowCutSlopeKnobAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeKnob),
    highCutFreqKnobAttachment(audioProcessor.apvts, "HiCut Freq", highCutFreqKnob),
    highCutSlopeKnobAttachment(audioProcessor.apvts, "HiCut Slope", highCutSlopeKnob)


{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    for (auto* component : getComponents())
    {
        addAndMakeVisible(component);
    }


    setSize (800, 500);
}

EQ_LiteAudioProcessorEditor::~EQ_LiteAudioProcessorEditor()
{
}

//==============================================================================
void EQ_LiteAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
   // g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void EQ_LiteAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    // Setting positions of my custom components    ~A
    auto bounds = getLocalBounds();
    auto graphicResponseArea = bounds.removeFromTop(bounds.getHeight() * 0.4);  // Reserving area for the response window   ~A

    auto lowCutFilterFreqArea = bounds.removeFromLeft(bounds.getWidth() * 0.2);
    //lowCutFilterFreqArea = lowCutFilterFreqArea.removeFromTop(lowCutFilterFreqArea.getHeight() * 0.84);
    auto highCutFilterFreqArea = bounds.removeFromRight(bounds.getWidth() * 0.25);
    auto band1FreqArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto band2FreqArea = bounds.removeFromLeft(bounds.getWidth() * 0.5);
    auto band3FreqArea = bounds;

    auto band1GainArea = band1FreqArea.removeFromBottom(band1FreqArea.getHeight() * 0.66);
    auto band1QArea = band1GainArea.removeFromBottom(band1GainArea.getHeight() * 0.5);
    auto band2GainArea = band2FreqArea.removeFromBottom(band2FreqArea.getHeight() * 0.66);
    auto band2QArea = band2GainArea.removeFromBottom(band2GainArea.getHeight() * 0.5);
    auto band3GainArea = band3FreqArea.removeFromBottom(band3FreqArea.getHeight() * 0.66);
    auto band3QArea = band3GainArea.removeFromBottom(band3GainArea.getHeight() * 0.5);

    auto lowCutFilterSlopeArea = lowCutFilterFreqArea.removeFromBottom(lowCutFilterFreqArea.getHeight() * 0.5);
    auto highCutFilterSlopeArea = highCutFilterFreqArea.removeFromBottom(highCutFilterFreqArea.getHeight() * 0.5);
    

    lowCutFreqKnob.setBounds(lowCutFilterFreqArea);
    highCutFreqKnob.setBounds(highCutFilterFreqArea);
    band1FreqKnob.setBounds(band1FreqArea);
    band2FreqKnob.setBounds(band2FreqArea);
    band3FreqKnob.setBounds(band3FreqArea);
    
    band1GainKnob.setBounds(band1GainArea);
    band1QKnob.setBounds(band1QArea);
    band2GainKnob.setBounds(band2GainArea);
    band2QKnob.setBounds(band2QArea);
    band3GainKnob.setBounds(band3GainArea);
    band3QKnob.setBounds(band3QArea);

    lowCutSlopeKnob.setBounds(lowCutFilterSlopeArea);
    highCutSlopeKnob.setBounds(highCutFilterSlopeArea);
    
 
}


std::vector<juce::Component*> EQ_LiteAudioProcessorEditor::getComponents()
{
    std::vector<juce::Component*> compVec{ &band1FreqKnob, &band2FreqKnob, &band3FreqKnob, &band1QKnob, &band2QKnob, &band3QKnob,
                                           &band1GainKnob, &band2GainKnob, &band3GainKnob, &lowCutFreqKnob, &highCutFreqKnob, &lowCutSlopeKnob,
                                           &highCutSlopeKnob};
    return compVec;
}