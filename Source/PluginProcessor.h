/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Declaring enum for low/hi cut slope choices    ~A
enum Slope
{
    Slope_12,
    Slope_24,
    Slope_36,
    Slope_48
};



// Adding data structure holding all the EQ parameters      ~A
struct ChainSettings
{
    float band1Freq{ 0 }, band1GainDB{ 0 }, band1Quality{1.f};
    float band2Freq{ 0 }, band2GainDB{ 0 }, band2Quality{ 1.f };
    float band3Freq{ 0 }, band3GainDB{ 0 }, band3Quality{ 1.f };
    float lowCutFreq{ 0 }, highCutFreq{ 0 };
    int lowCutSlope{ Slope::Slope_12 }, highCutSlope{ Slope::Slope_12 };
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

//          Moved those out of private to pass them to plugin editor to draw the spectrum    ~A

 // Creating aliases for convoluted namespaces   ~A
using Filter = juce::dsp::IIR::Filter<float>;
using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;                // 4 filters for 4 db/Oct settings  ~A
using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, Filter, Filter, CutFilter>;  // whole mono chain

// Declaring enum for clarity of filter names   ~A
enum ChainPositions
{
    LowCut,
    Band1,
    Band2,
    Band3,
    HighCut
};

using Coefficients = Filter::CoefficientsPtr;
void updateCoefficients(Coefficients& old, const Coefficients& replacement);


Coefficients makeBand1Filter(const ChainSettings& chainSettings, double sampleRate);
Coefficients makeBand2Filter(const ChainSettings& chainSettings, double sampleRate);
Coefficients makeBand3Filter(const ChainSettings& chainSettings, double sampleRate);

// Moved all these functions here to make them global   ~A

template<int Index, typename ChainType, typename CoefficientType>
void update(ChainType& chain, const CoefficientType& coefficients)
{
    updateCoefficients(chain.get<Index>().coefficients, coefficients[Index]);
    chain.setBypassed<Index>(false);
}

template<typename ChainType, typename CoefficientType>
void updateCutFilter(ChainType& cutType,
    const CoefficientType& cutCoefficients,
    const int& CutSlope)
{
    cutType.setBypassed<0>(true);     //  Initially bypassing all the slope options  ~A
    cutType.setBypassed<1>(true);
    cutType.setBypassed<2>(true);
    cutType.setBypassed<3>(true);

    // A trick to avoid repeated code - using reverse order instead of breaks and a template function declared above       ~A
    switch (CutSlope)
    {
    case Slope_48:
    {
        update<3>(cutType, cutCoefficients);
    }

    case Slope_36:
    {
        update<2>(cutType, cutCoefficients);
    }

    case Slope_24:
    {
        update<1>(cutType, cutCoefficients);
    }

    case Slope_12:
    {
        update<0>(cutType, cutCoefficients);
    }
    }
}



// Declaring this function inline not to confuse the compiler   ~A
inline auto makeLowCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                       sampleRate,
                                                                                       2 * (chainSettings.lowCutSlope + 1));
}


inline auto makeHighCutFilter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                     sampleRate,
                                                                                     2 * (chainSettings.highCutSlope + 1));
}

//==============================================================================
/**
*/
class EQ_LiteAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    EQ_LiteAudioProcessor();
    ~EQ_LiteAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();
    
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

private:

    MonoChain leftChain, rightChain;                                                             // 2 mono chains for stereo    ~A

    // Cleaning up the code via helper functions   ~A
    void updateBand1Filter(const ChainSettings& chainSettings);
    void updateBand2Filter(const ChainSettings& chainSettings);
    void updateBand3Filter(const ChainSettings& chainSettings);


    // Moved the commented out lines to global scope because they're needed for
    // drawing the response curve   ~A
    //using Coefficients = Filter::CoefficientsPtr;
    //static void updateCoefficients(Coefficients& old, const Coefficients& replacement);


 


    // Refactoring the code even more   ~A
    void updateLowCutFilters(const ChainSettings& chainSettings);
    void updateHighCutFilters(const ChainSettings& chainSettings);
    void updateFilters();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQ_LiteAudioProcessor)
};
