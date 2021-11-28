/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


ResponseCurveWindow::ResponseCurveWindow(EQ_LiteAudioProcessor& p) : audioProcessor(p)
{
    // Grabbing all the audio parameters and becoming a listener of them    ~A
    const auto& parameters = audioProcessor.getParameters();
    for (auto parameter : parameters)
    {
        parameter->addListener(this);
    }

    startTimerHz(60);
}

ResponseCurveWindow::~ResponseCurveWindow()
{
    // If we registered as a listener, we have to deregister in the destructor  ~A
    const auto& parameters = audioProcessor.getParameters();
    for (auto parameter : parameters)
    {
        parameter->removeListener(this);
    }
}

void ResponseCurveWindow::parameterValueChanged(int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}

void ResponseCurveWindow::timerCallback()
{
    if (parametersChanged.compareAndSetBool(false, true))
    {
        // Updating the monochain   ~A
        auto chainSettings = getChainSettings(audioProcessor.apvts);
        auto band1Coefficients = makeBand1Filter(chainSettings, audioProcessor.getSampleRate());
        auto band2Coefficients = makeBand2Filter(chainSettings, audioProcessor.getSampleRate());
        auto band3Coefficients = makeBand3Filter(chainSettings, audioProcessor.getSampleRate());
        auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
        auto highCutCoefficeints = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());

        updateCoefficients(monoChain.get<ChainPositions::Band1>().coefficients, band1Coefficients);
        updateCoefficients(monoChain.get<ChainPositions::Band2>().coefficients, band2Coefficients);
        updateCoefficients(monoChain.get<ChainPositions::Band3>().coefficients, band3Coefficients);
        updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
        updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficeints, chainSettings.highCutSlope);
        // Signaling a repaint  ~A
        repaint();
    }
}

void ResponseCurveWindow::paint(juce::Graphics& g)
{
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(Colours::black);
   
    auto graphicResponseArea = getLocalBounds();

    int w = graphicResponseArea.getWidth();

    auto& lowcut = monoChain.get<ChainPositions::LowCut>();
    auto& band1 = monoChain.get<ChainPositions::Band1>();
    auto& band2 = monoChain.get<ChainPositions::Band2>();
    auto& band3 = monoChain.get<ChainPositions::Band3>();
    auto& highcut = monoChain.get<ChainPositions::HighCut>();

    double sampleRate = audioProcessor.getSampleRate();

    // Creating a vector of doubles to be iterated thru that will be 
    // resized to the width of the display in pixels and contain magnitudes  ~A
    std::vector<double> magnitudes;
    magnitudes.resize(w);

    for (int i = 0; i < w; i++)
    {
        double magnitude = 1.f;
        // Using a helper function to change from pixel width range to frequency range ~A
        auto freq = mapToLog10(double(i) / w, 20.0, 20000.0);

        // Multiplying magnitude by every link of the chain IF it's not bypassed    ~A

        // 3 bands  ~A
        if (!monoChain.isBypassed<ChainPositions::Band1>())
            magnitude *= band1.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!monoChain.isBypassed<ChainPositions::Band2>())
            magnitude *= band2.coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!monoChain.isBypassed<ChainPositions::Band3>())
            magnitude *= band3.coefficients->getMagnitudeForFrequency(freq, sampleRate);

        // 4 slope options of low and high cut ~A
        if (!lowcut.isBypassed<0>())
            magnitude *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<1>())
            magnitude *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<2>())
            magnitude *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!lowcut.isBypassed<3>())
            magnitude *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        if (!highcut.isBypassed<0>())
            magnitude *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<1>())
            magnitude *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<2>())
            magnitude *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        if (!highcut.isBypassed<3>())
            magnitude *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);

        magnitudes[i] = Decibels::gainToDecibels(magnitude);
    }

    // !!!!!!!!!UZYJ TEGO DO NARYSOWANIA LADNYCH OBWODEK ITP!!!!!!!!!!!!!
    Path responseCurve;
    const double outputMin = graphicResponseArea.getBottom();
    const double outputMax = graphicResponseArea.getY();

    // Declaring a helper lambda    ~A
    auto map = [outputMin, outputMax](double input)
    {
        return jmap(input, -24.0, 24.0, outputMin, outputMax);
    };

    // Starting a response subpath on the leftmost pixel given the first magnitude value from the vector    ~A
    responseCurve.startNewSubPath(graphicResponseArea.getX(), map(magnitudes.front()));

    // Continuing the response curve by drawing a new line with different bearing from pixel to pixel   ~A
    for (size_t i = 1; i < magnitudes.size(); i++)
    {
        responseCurve.lineTo(graphicResponseArea.getX() + i, map(magnitudes.at(i)));
    }


    // Drawing an outline and the path   ~A
    g.setColour(Colours::burlywood);
    g.drawRoundedRectangle(graphicResponseArea.toFloat(), 10.f, 2.f);
    g.setColour(Colours::azure);
    g.strokePath(responseCurve, PathStrokeType(2.f));

    // Drawing the rest of the GUI details  ~A
    //g.setColour(Colours::beige);
    //juce::Rectangle<float> lcOutline(200.f, 200.f, 200.f, 200.f);
    //g.drawRoundedRectangle(lcOutline, 50.f, 5.f);
}


//==============================================================================
EQ_LiteAudioProcessorEditor::EQ_LiteAudioProcessorEditor(EQ_LiteAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    responseCurveWindow(audioProcessor),
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
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (Colours::black);
   

    //``````````````// Drawing the rest of the GUI details  ~A
    //g.setColour(Colours::beige);
    //juce::Rectangle<float> lcOutline(200.f, 200.f, 200.f, 200.f);
    //g.drawRoundedRectangle(lcOutline, 50.f, 5.f);
    




}

void EQ_LiteAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    // Setting positions of my custom components    ~A
    auto bounds = getLocalBounds();
    auto graphicResponseArea = bounds.removeFromTop(bounds.getHeight() * 0.4);  // Reserving area for the response window   ~A
    responseCurveWindow.setBounds(graphicResponseArea);

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
                                           &highCutSlopeKnob, &responseCurveWindow};
    return compVec;
}