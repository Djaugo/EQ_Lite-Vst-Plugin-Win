/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"


// FFT data generator producing FFT data from an audio buffer  ~A

enum FFTOrder
{
    order2048 = 11,
    order4096 = 12,
    order8192 = 13
};

template<typename BlockType>
struct FFTDataGenerator
{
    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
    {
        const auto fftSize = getFFTSize();

        fftData.assign(fftData.size(), 0);
        auto* readIndex = audioData.getReadPointer(0);
        std::copy(readIndex, readIndex + fftSize, fftData.begin());

        // Applying a windowing function to our data   ~A
        window->multiplyWithWindowingTable(fftData.data(), fftSize);

        // Rendering FFT data   ~A
        forwardFFT->performFrequencyOnlyForwardTransform(fftData.data());

        int numBins = (int)fftSize / 2;

        // Normalizing fft values   ~A
        for (int i = 0; i < numBins; ++i)
        {
            fftData[i] /= (float)numBins;
        }
        // Converting them to decibels  ~A
        for (int i = 0; i < numBins; ++i)
        {
            fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInfinity);
        }
        fftDataFifo.push(fftData);
    }

    void changeOrder(FFTOrder newOrder)
    {
        order = newOrder;
        auto fftSize = getFFTSize();

        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::blackmanHarris);

        fftData.clear();
        fftData.resize(fftSize * 2, 0);

        fftDataFifo.prepare(fftData.size());
    }
    //===================================================================
    int getFFTSize() const { return 1 << order; }
    int getNumAvailableFFTDataBlokcs() { return fftDataFifo.pull(fftData); }
    //===================================================================
    bool getFFTData(BlockType& fftData) { return fftDataFifo.pull(fftData); }
private:
    FFTOrder order;
    BlockType fftData;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;

    Fifo<BlockType> fftDataFifo;

};

// Spectrum analyser path generator based on FFT data   ~A
template<typename PathType>
struct AnalyzerPathGenerator
{
    // Converts 'renderData[]' into a juce::Path
    void generatePath(const std::vector<float>& renderData,
                      juce::Rectangle<float> fftBounds,
                      int fftSize,
                      float binWidth,
                      float negativeInfinity)
    {
        auto top = fftBounds.getY();
        auto bottom = fftBounds.getHeight();
        auto width = fftBounds.getWidth();

        int numBins = (int)fftSize / 2;

        PathType p;
        p.preallocateSpace(3 * (int)fftBounds.getWidth());

        auto map = [bottom, top, negativeInfinity](float v)
        {
            return juce::jmap(v,
                              negativeInfinity, 0.f, 
                              float(bottom), top);
        };

        auto y = map(renderData[0]);

        jassert(!std::isnan(y) && !std::isinf(y));

        p.startNewSubPath(0, y);

        const int pathResolution = 2;

        for (int binNum = 1; binNum < numBins; binNum += pathResolution)
        {
            y = map(renderData[binNum]);

            jassert(!std::isnan(y) && !std::isinf(y));

            if (!std::isnan(y) && !std::isinf(y))
            {
                auto binFreq = binNum * binWidth;
                auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
                int binX = std::floor(normalizedBinX * width);
                p.lineTo(binX, y);
            }
        }
        pathFifo.push(p);
    }

    int getNumPathsAvailable() const
    {
        return pathFifo.getNumAvailableForReading();
    }

    bool getPath(PathType& path)
    {
        return pathFifo.pull(path);
    }
private:
    Fifo<PathType> pathFifo;
};



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

struct PathProducer
{
    PathProducer(SingleChannelSampleFifo<EQ_LiteAudioProcessor::BlockType>& scsf) :
    leftChannelFifo(&scsf)
    {
        /*
         If samplerate is 48000 then the resolution for the order of 2048
         equals 48000 / 2048 = 23 Hz. That might make the low end resolution
         a bit lacking. Is there a way to gradually lower the resolution as the
         sound frequence increases?      ~A
         */

        leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
        monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
    }
    void process(juce::Rectangle<float> fftBound, double sampleRate);
    juce::Path getPath() { return leftChannelFFTPath; }
    
private:
    // Doing the convoluted spectrum analyser work  ~A
    SingleChannelSampleFifo<EQ_LiteAudioProcessor::BlockType>* leftChannelFifo;

    juce::AudioBuffer<float> monoBuffer;

    FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;

    AnalyzerPathGenerator<juce::Path> pathProducer;

    juce::Path leftChannelFFTPath;
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

    // Creating a spectrum analyser path producer for both channels     ~A
    PathProducer leftPathProducer, rightPathProducer;

  

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
