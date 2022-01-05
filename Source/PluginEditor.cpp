/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/


#include "PluginProcessor.h"
#include "PluginEditor.h"

void LookAndFeel::drawRotarySlider(juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    using namespace juce;

    // Drawing the knob circles     ~A
    auto bounds = Rectangle<float>(x, y, width, height);

    g.setColour(Colour(38u, 38u, 38u));
    g.fillEllipse(bounds);

    g.setColour(Colour(220u, 220u, 220u));
    g.drawEllipse(bounds, 5.f);

    // Drawing the knob rectangle   ~A
    auto center = bounds.getCentre();
    Path p1;
    Path p2;
    
    Rectangle<float> r1;
    r1.setLeft(center.getX() - 6);
    r1.setRight(center.getX() + 6);
    r1.setTop(bounds.getY());
    r1.setBottom(center.getY()-15);

    
    Rectangle<float> r2;
    r2.setLeft(center.getX() - 4);
    r2.setRight(center.getX() + 4);
    r2.setTop(bounds.getY() +2);
    r2.setBottom(center.getY() - 17);

    p1.addRectangle(r1);
    p2.addRectangle(r2);
    jassert(rotaryStartAngle < rotaryEndAngle);
    
    auto sliderAngleRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);
    p1.applyTransform(AffineTransform().rotated(sliderAngleRad, center.getX(), center.getY()));
    p2.applyTransform(AffineTransform().rotated(sliderAngleRad, center.getX(), center.getY()));
    
    g.setColour(Colour(0u, 0u, 0u));
    g.fillPath(p1);
    g.setColour(Colour(250u, 250u, 250u));
    g.fillPath(p2);
    

}

// Drawing the toggle bypass button
void LookAndFeel::drawToggleButton(juce::Graphics& g,
                                   juce::ToggleButton& toggleButton,
                                   bool shouldDrawButtonAsHighlighted,
                                   bool ShouldDrawButtonAsDown)
{
    using namespace juce;
    Path powerButton;
    auto bounds = toggleButton.getLocalBounds();

    auto size = jmin(bounds.getWidth(), bounds.getHeight()) - 6;
    auto r = bounds.withSizeKeepingCentre(size, size).toFloat();

    float ang = 30.f;
    size -= 6;

    powerButton.addCentredArc(r.getCentreX(), 
                              r.getCentreY(), 
                              size * 0.5, 
                              size * 0.5, 
                              0.f, 
                              degreesToRadians(ang), 
                              degreesToRadians(360.f - ang),
                              true);
    powerButton.startNewSubPath(r.getCentreX(), r.getY());
    powerButton.lineTo(r.getCentre());

    PathStrokeType pst(2.f, PathStrokeType::JointStyle::curved);

    auto color = toggleButton.getToggleState() ? Colours::dimgrey : Colours::azure;

    g.setColour(color);
    g.strokePath(powerButton, pst);
    g.drawEllipse(r, 1);
}

// Implementing paint function to draw the custom knobs     ~A
void MyEQKnob1::paint(juce::Graphics& g)
{
    using namespace juce;
    

    // Making the knobs have starting value at 7 o'clock and ending value at 5 o'clock  ~A
    auto startAngle = degreesToRadians(180.f + 45.f);
    auto endAngle = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

    auto range = getRange();

    auto sliderBounds = getSliderBounds();

   /* g.setColour(Colours::red);
    g.drawRect(getLocalBounds());
    g.setColour(Colours::orangered);
    g.drawRect(sliderBounds);*/

    getLookAndFeel().drawRotarySlider(g,
                                      sliderBounds.getX(),
                                      sliderBounds.getY(),
                                      sliderBounds.getWidth(),
                                      sliderBounds.getHeight(),
                                      jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0),
                                      startAngle,
                                      endAngle,
                                      *this);

    // Drawing the min max values for each knob     ~A
    auto knobCenter = sliderBounds.toFloat().getCentre();
    auto radius = sliderBounds.getWidth() * 0.5f;
    g.setColour(Colour(220u, 220u, 220u));
    g.setFont(getTextHeight());
    for (int i = 0; i < labelsArray.size(); i++)
    {
        auto position = labelsArray[i].position;
        jassert(0.f <= position);
        jassert(position <= 1.f);
        auto angle = jmap(position, 0.f, 1.f, startAngle, endAngle);

        auto c = knobCenter.getPointOnCircumference(radius + getTextHeight() * 0.5f + 1, angle);
        Rectangle<float> r;
        juce::String str = labelsArray[i].label;
        r.setSize(g.getCurrentFont().getStringWidth(str), getTextHeight());
        r.setCentre(c);
        r.setY(r.getY() + getTextHeight());

        g.drawFittedText(str, r.toNearestInt(), juce::Justification::verticallyCentred, 1);
    }

   
}

juce::Rectangle<int> MyEQKnob1::getSliderBounds() const
{
    auto bounds = getLocalBounds();
    
    juce::Rectangle<int> r;
    r.setSize(70, 70);
    r.setCentre(bounds.getCentreX(), bounds.getCentreY());
    return r;
}


ResponseCurveWindow::ResponseCurveWindow(EQ_LiteAudioProcessor& p) :
    audioProcessor(p),
    leftPathProducer(audioProcessor.leftChannelFifo),
    rightPathProducer(audioProcessor.rightChannelFifo)
{
    // Grabbing all the audio parameters and becoming a listener of them    ~A
    const auto& parameters = audioProcessor.getParameters();
    for (auto parameter : parameters)
    {
        parameter->addListener(this);
    }

 

    updateChain();

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

void PathProducer::process(juce::Rectangle<float> fftBounds, double sampleRate)
{
    juce::AudioBuffer<float> tempIncomingBuffer;

    while (leftChannelFifo->getNumCompleteBuffersAvailable() > 0)
    {
        if (leftChannelFifo->getAudioBuffer(tempIncomingBuffer))
        {
            auto size = tempIncomingBuffer.getNumSamples();

            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, 0),
                monoBuffer.getReadPointer(0, size),
                monoBuffer.getNumSamples() - size);
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                tempIncomingBuffer.getReadPointer(0, 0),
                size);

            leftChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);
        }
    }

    // If there are FFT data buffers to pull and we can pull them, generate a path  ~A
    
    const auto fftSize = leftChannelFFTDataGenerator.getFFTSize();
    const auto binWidth = sampleRate / (double)fftSize;

    while (leftChannelFFTDataGenerator.getNumAvailableFFTDataBlokcs() > 0)
    {
        std::vector<float> fftData;
        if (leftChannelFFTDataGenerator.getFFTData(fftData))
        {
            pathProducer.generatePath(fftData, fftBounds, fftSize, binWidth, -48.f);
        }
    }

    // While there are paths that can be pulled, pull as many as possible,
    // we can only display the most recent path ~A
    while (pathProducer.getNumPathsAvailable())
    {
        pathProducer.getPath(leftChannelFFTPath);
    }
}

void ResponseCurveWindow::timerCallback()
{
    
    auto fftBounds = getAnalysisArea().toFloat();
    fftBounds.removeFromLeft(20);
    auto sampleRate = audioProcessor.getSampleRate();

    leftPathProducer.process(fftBounds, sampleRate);
    rightPathProducer.process(fftBounds, sampleRate);
   

    if (parametersChanged.compareAndSetBool(false, true))
    {
        // Updating the monochain   ~A
        updateChain();
        // Signaling a repaint  ~A
        //repaint();
    }
    // Changed the repaint call from only when parameters changed to consant
    // due to the need of painting the spectrum    ~A
    repaint();
}

void ResponseCurveWindow::updateChain()
{
    auto chainSettings = getChainSettings(audioProcessor.apvts);

    monoChain.setBypassed<ChainPositions::LowCut>(chainSettings.lowCutBypassed);
    monoChain.setBypassed<ChainPositions::Band1>(chainSettings.band1Bypassed);
    monoChain.setBypassed<ChainPositions::Band2>(chainSettings.band2Bypassed);
    monoChain.setBypassed<ChainPositions::Band3>(chainSettings.band3Bypassed);
    monoChain.setBypassed<ChainPositions::HighCut>(chainSettings.highCutBypassed);
    

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
}

void ResponseCurveWindow::paint(juce::Graphics& g)
{
    using namespace juce;
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(Colour(0u, 0u, 0u));
   
    g.drawImage(responseBackground, getLocalBounds().toFloat());

    auto graphicResponseArea = getAnalysisArea();

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
        if (!monoChain.isBypassed<ChainPositions::LowCut>())
        {
            if (!lowcut.isBypassed<0>())
                magnitude *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<1>())
                magnitude *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<2>())
                magnitude *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!lowcut.isBypassed<3>())
                magnitude *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
       
        if (!monoChain.isBypassed<ChainPositions::HighCut>())
        {
            if (!highcut.isBypassed<0>())
                magnitude *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<1>())
                magnitude *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<2>())
                magnitude *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
            if (!highcut.isBypassed<3>())
                magnitude *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, sampleRate);
        }
        

        magnitudes[i] = Decibels::gainToDecibels(magnitude);
    }

   
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
        // Trying some walkaround to fix the response cureve's drawing area problem     ~A
        
        // Declaring bools for clarity of the subsequent if-chain   ~A
         bool isCurrMagWithinBounds{ false };
         bool isPrevMagWithinBounds{ false };

         if (magnitudes.at(i) > -25 && magnitudes.at(i) < 25)
             isCurrMagWithinBounds = true;
         if (magnitudes.at(i - 1) > -25 && magnitudes.at(i - 1) < 25)
             isPrevMagWithinBounds = true;

         // Walkaround logic to keep the response curve vertically within shrunken window bounds    ~A

         if (isCurrMagWithinBounds && isPrevMagWithinBounds)
         {
             responseCurve.lineTo(graphicResponseArea.getX() + i, map(magnitudes.at(i)));
         }
         else if (isCurrMagWithinBounds && !isPrevMagWithinBounds)
         {
             responseCurve.startNewSubPath(graphicResponseArea.getX() + i, map(magnitudes.at(i)));
         }

        
        
    }

    // Drawing the spectrum analyser    ~A
    auto leftChannelFFTPath = leftPathProducer.getPath();
    leftChannelFFTPath.applyTransform(AffineTransform().translation(graphicResponseArea.getX(), graphicResponseArea.getY()));
    
    g.setColour(Colours::lightpink);
    g.strokePath(leftChannelFFTPath, PathStrokeType(1));

    auto rightChannelFFTPath = rightPathProducer.getPath();
    rightChannelFFTPath.applyTransform(AffineTransform().translation(graphicResponseArea.getX(), graphicResponseArea.getY()));
    
    g.setColour(Colours::lightyellow);
    g.strokePath(rightChannelFFTPath, PathStrokeType(1));

    // Drawing an outline and the path   ~A
    g.setColour(Colours::burlywood);
    g.drawRoundedRectangle(getRenderArea().toFloat(), 10.f, 2.f);
    auto color = Colours::azure;
    //auto color = EQ_LiteAudioProcessorEditor::A ? Colours::dimgrey : Colours::azure;
    g.setColour(color);
    g.strokePath(responseCurve, PathStrokeType(2.f));

    
}

void ResponseCurveWindow::resized()
{
    using namespace juce;
    responseBackground = Image(Image::PixelFormat::RGB, getWidth(), getHeight(), true);
    
    Graphics g(responseBackground);

    // Creating an array holding the frequencies that will be marked on the response window grid    ~A
    Array<float> frequencies
    {
        20, 50, 100,
        200, 500, 1000,
        2000, 5000, 10000,
        20000
    };
    
    // Declaring the new limited drawing boundaries     ~A
    auto analysisArea = getAnalysisArea();
    auto left = analysisArea.getX();
    auto right = analysisArea.getRight();
    auto top = analysisArea.getY();
    auto bottom = analysisArea.getBottom();
    auto width = analysisArea.getWidth();
    // Mapping the frequencies into a normalised range and containing them 
    // within limited boundaries   ~A
    Array<float> xs;
    g.setColour(Colours::grey);
    for (auto freq : frequencies)
    {
        auto normX = mapFromLog10(freq, 20.f, 20000.f);
        auto newX = left + width * normX;
        xs.add(newX);

        if (freq != 20 && freq != 20000)
        g.drawVerticalLine(newX, top, bottom);
    }


    // Doing the same but for the horizontal gain grid lines    ~A
    Array<float> gains{-24, -12, 0, 12, 24};
    for (auto gaindB : gains)
    {
        auto y = jmap(gaindB, -24.f, 24.f, float(bottom), float(top));
        g.setColour(gaindB == 0 ? Colours::burlywood : Colours::darkgrey);
        if(gaindB == 24 || gaindB == -24)
        g.drawHorizontalLine(y, left+3, right-3);
        else
        g.drawHorizontalLine(y, left, right);
    }

    // Drawing the grid labels  ~A
    g.setColour(Colours::lightgrey);
    const int fontHeight = 10;
    g.setFont(fontHeight);

    for (int i = 0; i < frequencies.size(); i++)
    {
        auto f = frequencies[i];
        auto x = xs[i];
        
        bool addK = false;
        String str;
        if (f > 999.f)
        {
            addK = true;
            f /= 1000.f;
        }
        str << f << " ";
        if (addK)
            str << "k";
        str << "Hz";

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setCentre(x, 0);
        r.setY(bottom+6);

        g.drawFittedText(str, r, juce::Justification::centred, 1);
    }
    
    // RHS gain labels for the response curve ~A
    for (auto gaindB : gains)
    {
        auto y = jmap(gaindB, -24.f, 24.f, float(bottom), float(top));
        String str;
        if (gaindB > 0)
            str << "+";
        str << gaindB;
        str <<" dB";

        auto textWidth = g.getCurrentFont().getStringWidth(str);

        Rectangle<int> r;
        r.setSize(textWidth, fontHeight);
        r.setX(getWidth() - textWidth);
        r.setCentre(r.getCentreX(), y);

        g.setColour(gaindB == 0 ? Colours::burlywood : Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::centred, 1);

        // LHS gain labels for the spectrum display     ~A
        str.clear();
            // Silly walkaround coz I can't force the  0 dB label to be 
            // right-oriented with the justification flag below  ~A
        if ((gaindB - 24.f) == 0)
        str << "   " << (gaindB - 24.f) << " dB";
        else
        str << (gaindB - 24.f) << " dB";

        r.setX(1);
        textWidth = g.getCurrentFont().getStringWidth(str);
        r.setSize(textWidth, fontHeight);
        g.setColour(Colours::lightgrey);
        g.drawFittedText(str, r, juce::Justification::right, 1);
    }


}

juce::Rectangle<int> ResponseCurveWindow::getRenderArea()
{
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(12);
    bounds.removeFromTop(5);
    bounds.removeFromLeft(35);
    bounds.removeFromRight(35); 

    return bounds;
}

juce::Rectangle<int> ResponseCurveWindow::getAnalysisArea()
{
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(4);
 
    return bounds;
}

//==============================================================================
EQ_LiteAudioProcessorEditor::EQ_LiteAudioProcessorEditor(EQ_LiteAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    band1FreqKnob(*audioProcessor.apvts.getParameter("Band1 Freq"), " Hz"),
    band2FreqKnob(*audioProcessor.apvts.getParameter("Band2 Freq"), " Hz"),
    band3FreqKnob(*audioProcessor.apvts.getParameter("Band3 Freq"), " Hz"),
    band1GainKnob(*audioProcessor.apvts.getParameter("Band1 Gain"), " dB"),
    band2GainKnob(*audioProcessor.apvts.getParameter("Band2 Gain"), " dB"),
    band3GainKnob(*audioProcessor.apvts.getParameter("Band3 Gain"), " dB"),
    band1QKnob(*audioProcessor.apvts.getParameter("Band1 Quality"), ""),
    band2QKnob(*audioProcessor.apvts.getParameter("Band2 Quality"), ""),
    band3QKnob(*audioProcessor.apvts.getParameter("Band3 Quality"), ""),
    lowCutFreqKnob(*audioProcessor.apvts.getParameter("LowCut Freq"), " Hz"),
    lowCutSlopeKnob(*audioProcessor.apvts.getParameter("LowCut Slope"), " dB/Oct"),
    highCutFreqKnob(*audioProcessor.apvts.getParameter("HiCut Freq"), " Hz"),
    highCutSlopeKnob(*audioProcessor.apvts.getParameter("HiCut Slope"), " dB/Oct"),

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
    highCutSlopeKnobAttachment(audioProcessor.apvts, "HiCut Slope", highCutSlopeKnob),

    lowcutBypassButtonAttachment(audioProcessor.apvts, "LowCut Bypassed", lowcutBypassButton),
    highcutBypassButtonAttachment(audioProcessor.apvts, "HighCut Bypassed", highcutBypassButton),
    band1BypassButtonAttachment(audioProcessor.apvts, "Band1 Bypassed", band1BypassButton),
    band2BypassButtonAttachment(audioProcessor.apvts, "Band2 Bypassed", band2BypassButton),
    band3BypassButtonAttachment(audioProcessor.apvts, "Band3 Bypassed", band3BypassButton),
    allBypassButtonAttachment(audioProcessor.apvts, "All Bypassed", allBypassButton),
    analyzerEnabledButtonAttachment(audioProcessor.apvts, "Analyzer Enabled", analyzerEnabledButton)



{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.

    // Initializing knobs' labels to proper values  ~A
    band1FreqKnob.labelsArray.add({ 0.f, "20 Hz" });
    band1FreqKnob.labelsArray.add({ 1.f, "20 kHz" });
    band2FreqKnob.labelsArray.add({ 0.f, "20 Hz" });
    band2FreqKnob.labelsArray.add({ 1.f, "20 kHz" });
    band3FreqKnob.labelsArray.add({ 0.f, "20 Hz" });
    band3FreqKnob.labelsArray.add({ 1.f, "20 kHz" });
    band1GainKnob.labelsArray.add({ 0.f, "-24 dB" });
    band1GainKnob.labelsArray.add({ 1.f, "24 dB" });
    band2GainKnob.labelsArray.add({ 0.f, "-24 dB" });
    band2GainKnob.labelsArray.add({ 1.f, "24 dB" });
    band3GainKnob.labelsArray.add({ 0.f, "-24 dB" });
    band3GainKnob.labelsArray.add({ 1.f, "24 dB" });
    band1QKnob.labelsArray.add({ 0.f, "0.1" });
    band1QKnob.labelsArray.add({ 1.f, "10" });
    band2QKnob.labelsArray.add({ 0.f, "0.1" });
    band2QKnob.labelsArray.add({ 1.f, "10" });
    band3QKnob.labelsArray.add({ 0.f, "0.1" });
    band3QKnob.labelsArray.add({ 1.f, "10" });
    lowCutFreqKnob.labelsArray.add({ 0.f, "20 Hz" });
    lowCutFreqKnob.labelsArray.add({ 1.f, "20 kHz" });
    lowCutSlopeKnob.labelsArray.add({ 0.f, "12 dB/Oct" });
    lowCutSlopeKnob.labelsArray.add({ 1.f, "48 dB/Oct" });
    highCutFreqKnob.labelsArray.add({ 0.f, "20 Hz" });
    highCutFreqKnob.labelsArray.add({ 1.f, "20 kHz" });
    highCutSlopeKnob.labelsArray.add({ 0.f, "12 dB/Oct" });
    highCutSlopeKnob.labelsArray.add({ 1.f, "48 dB/Oct" });


    for (auto* component : getComponents())
    {
        addAndMakeVisible(component);
    }

    lowcutBypassButton.setLookAndFeel(&lnf);
    band1BypassButton.setLookAndFeel(&lnf);
    band2BypassButton.setLookAndFeel(&lnf);
    band3BypassButton.setLookAndFeel(&lnf);
    highcutBypassButton.setLookAndFeel(&lnf);
    allBypassButton.setLookAndFeel(&lnf);
    


    // Size of the whole plugin window     ~A
    setSize (800, 600);
    
}

EQ_LiteAudioProcessorEditor::~EQ_LiteAudioProcessorEditor()
{
    lowcutBypassButton.setLookAndFeel(nullptr);
    band1BypassButton.setLookAndFeel(nullptr);
    band2BypassButton.setLookAndFeel(nullptr);
    band3BypassButton.setLookAndFeel(nullptr);
    highcutBypassButton.setLookAndFeel(nullptr);
    allBypassButton.setLookAndFeel(nullptr);

}

//==============================================================================
void EQ_LiteAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    //g.fillAll(Colours::black);
    
    /*juce::File backgroundTexture;
    juce::FileInputStream(&backgroundTexture);
    juce::PNGImageFormat image;
    image.findImageFormatForStream()*/
    
     
    
    // Writing the band names
    auto textBounds = getLocalBounds();
    juce::Rectangle<int> textR;
    textR.setSize(690, 300);
    textR.setCentre(textBounds.getCentreX(), 215);
    g.setColour(Colour(220u, 220u, 220u));
    g.setFont(20);
    g.drawFittedText("Low cut                            Peak band 1                         Peak band 2                        Peak band 3                          High cut",
                      textR, juce::Justification::Flags::left, 1, 0.5f);

    //``````````````// Drawing the rest of the GUI details  ~A
    //g.setColour(Colours::beige);
    //juce::Rectangle<float> lcOutline(200.f, 200.f, 200.f, 200.f);
    //g.drawRoundedRectangle(lcOutline, 50.f, 5.f);
   
    Image backgroundTexture = ImageCache::getFromMemory(BinaryData::basictexture_png, BinaryData::basictexture_pngSize);
    g.setOpacity(1.0f);
    g.drawImageAt(backgroundTexture, 0, 0, false);
    



}

void EQ_LiteAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..

    // Setting positions of my custom components    ~A
    auto bounds = getLocalBounds();
    auto graphicResponseArea = bounds.removeFromTop(bounds.getHeight() * 0.3);  // Reserving area for the response window   ~A
    responseCurveWindow.setBounds(graphicResponseArea);
    

    auto lowCutFilterFreqArea = bounds.removeFromLeft(bounds.getWidth() * 0.2);
    auto lowCutFilterSlopeArea = lowCutFilterFreqArea.removeFromBottom(lowCutFilterFreqArea.getHeight() * 0.5);
    lowCutFilterFreqArea.setTop(200);
    lowCutFilterFreqArea.setBottom(350);
    lowCutFilterSlopeArea.setTop(325);
    lowCutFilterSlopeArea.setBottom(475);

    auto highCutFilterFreqArea = bounds.removeFromRight(bounds.getWidth() * 0.25);
    auto highCutFilterSlopeArea = highCutFilterFreqArea.removeFromBottom(highCutFilterFreqArea.getHeight() * 0.5);
    highCutFilterFreqArea.setTop(200);
    highCutFilterFreqArea.setBottom(350);
    highCutFilterSlopeArea.setTop(325);
    highCutFilterSlopeArea.setBottom(475);

    auto band1FreqArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto band1GainArea = band1FreqArea.removeFromBottom(band1FreqArea.getHeight() * 0.66);
    auto band1QArea = band1GainArea.removeFromBottom(band1GainArea.getHeight() * 0.5);
    band1FreqArea.setTop(200);
    band1FreqArea.setBottom(350);
    band1GainArea.setTop(325);
    band1GainArea.setBottom(475);
    band1QArea.setTop(450);
    band1QArea.setBottom(600);

    auto band2FreqArea = bounds.removeFromLeft(bounds.getWidth() * 0.5);
    auto band2GainArea = band2FreqArea.removeFromBottom(band2FreqArea.getHeight() * 0.66);
    auto band2QArea = band2GainArea.removeFromBottom(band2GainArea.getHeight() * 0.5);
    band2FreqArea.setTop(200);
    band2FreqArea.setBottom(350);
    band2GainArea.setTop(325);
    band2GainArea.setBottom(475);
    band2QArea.setTop(450);
    band2QArea.setBottom(600);

    auto band3FreqArea = bounds;
    auto band3GainArea = band3FreqArea.removeFromBottom(band3FreqArea.getHeight() * 0.66);
    auto band3QArea = band3GainArea.removeFromBottom(band3GainArea.getHeight() * 0.5);
    band3FreqArea.setTop(200);
    band3FreqArea.setBottom(350);
    band3GainArea.setTop(325);
    band3GainArea.setBottom(475);
    band3QArea.setTop(450);
    band3QArea.setBottom(600);

    lowCutFreqKnob.setBounds(lowCutFilterFreqArea);
    lowCutSlopeKnob.setBounds(lowCutFilterSlopeArea);
    highCutFreqKnob.setBounds(highCutFilterFreqArea);
    highCutSlopeKnob.setBounds(highCutFilterSlopeArea);

    band1FreqKnob.setBounds(band1FreqArea);
    band1GainKnob.setBounds(band1GainArea);
    band1QKnob.setBounds(band1QArea);

    band2FreqKnob.setBounds(band2FreqArea);
    band2GainKnob.setBounds(band2GainArea);
    band2QKnob.setBounds(band2QArea);

    band3FreqKnob.setBounds(band3FreqArea);
    band3GainKnob.setBounds(band3GainArea);
    band3QKnob.setBounds(band3QArea);
    
    auto lowCutBypassArea = lowCutFilterFreqArea;
    lowCutBypassArea.setTop(205);           
    lowCutBypassArea.setBottom(230);
    lowCutBypassArea.setLeft(30);
    lowCutBypassArea.setRight(55);

    auto band1BypassArea = band1FreqArea;
    band1BypassArea.setTop(205);
    band1BypassArea.setBottom(230);
    band1BypassArea.setLeft(180);
    band1BypassArea.setRight(205);

    auto band2BypassArea = band2FreqArea;
    band2BypassArea.setTop(205);
    band2BypassArea.setBottom(230);
    band2BypassArea.setLeft(345);
    band2BypassArea.setRight(370);

    auto band3BypassArea = band3FreqArea;
    band3BypassArea.setTop(205);
    band3BypassArea.setBottom(230);
    band3BypassArea.setLeft(505);
    band3BypassArea.setRight(530);

    auto highCutBypassArea = highCutFilterFreqArea;
    highCutBypassArea.setTop(205);
    highCutBypassArea.setBottom(230);
    highCutBypassArea.setLeft(675);
    highCutBypassArea.setRight(700);

    auto allBypassArea = lowCutFilterSlopeArea;
    allBypassArea.setTop(515);
    allBypassArea.setBottom(560);
    allBypassArea.setLeft(20);
    allBypassArea.setRight(65);

    lowcutBypassButton.setBounds(lowCutBypassArea);
    highcutBypassButton.setBounds(highCutBypassArea);
    band1BypassButton.setBounds(band1BypassArea);
    band2BypassButton.setBounds(band2BypassArea);
    band3BypassButton.setBounds(band3BypassArea);
    allBypassButton.setBounds(allBypassArea);
    
    
  
}

std::vector<juce::Component*> EQ_LiteAudioProcessorEditor::getComponents()
{
    std::vector<juce::Component*> compVec{ &band1FreqKnob, &band2FreqKnob, &band3FreqKnob, &band1QKnob, &band2QKnob, &band3QKnob,
                                           &band1GainKnob, &band2GainKnob, &band3GainKnob, &lowCutFreqKnob, &highCutFreqKnob, &lowCutSlopeKnob,
                                           &highCutSlopeKnob, &responseCurveWindow,
    
                                           &lowcutBypassButton, &band1BypassButton, &band2BypassButton, &band3BypassButton,
                                           &highcutBypassButton, &allBypassButton, &analyzerEnabledButton};
    return compVec;
}