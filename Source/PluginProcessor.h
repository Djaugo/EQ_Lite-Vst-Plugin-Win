/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>

// A fifo the GUI thread will use to retrieve the blocks the single channel fifo produced   ~A
template<typename T>
struct Fifo
{
    void prepare(int numChannels, int numSamples)
    {
        static_assert(std::is_same_v<T, juce::AudioBuffer<float>>,
            "prepare(numChannels, numSamples) should only be used when fifo is holding juce::AudioBuffer<float>");
        for (auto& buffer : buffers)
        {
            buffer.setSize(numChannels,
                           numSamples,
                           false,       // clear everything?
                           true,        // including the extra space?
                           true);       // avoiding reallocating if you can?
            buffer.clear();
        }
    }

    void prepare(size_t numElements)
    {
        static_assert(std::is_same_v<T, std::vector<float>>,
            "prepare(numElements) should only be used when fifo is holding std::vector<float>");
        for (auto buffer : buffers)
        {
            buffer.clear();
            buffer.resize(numElements, 0);
        }
    }

    bool push(const T& t)
    {
        auto write = fifo.write(1);
        if (write.blockSize1 > 0)
        {
            buffers[write.startIndex1] = t;
            return true;
        }
        return false;
    }

    bool pull(T& t)
    {
        auto read = fifo.read(1);
        if (read.blockSize1 > 0)
        {
            t = buffers[read.startIndex1];
            return true;
        }
        return false;
    }

    int getNumAvailableForReading() const
    {
        return fifo.getNumReady();
    }
private:
    static constexpr int Capacity = 30;
    std::array<T, Capacity> buffers;
    juce::AbstractFifo fifo{ Capacity };
};

// Declaring enum for R and L channels  ~A
enum Channel
{
    Right,
    Left

};

// The FFT algorithm requires a fixed number of samples. The host is passing
// in buffers that vary in sample sizes. Collecting them in blocks of fixed sample
// sizes is possible by using the below class.     ~A

template<typename BlockType>
struct SingleChannelSampleFifo
{
    SingleChannelSampleFifo(Channel ch) : channelToUse(ch)
    {
        prepared.set(false);
    }

    void update(const BlockType& buffer)
    {
        jassert(prepared.get());
        jassert(buffer.getNumChannels() > channelToUse);
        auto* channelPtr = buffer.getReadPointer(channelToUse);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            pushNextSampleIntoFifo(channelPtr[i]);
        }
    }

    void prepare(int bufferSize)
    {
        prepared.set(false);
        size.set(bufferSize);

        bufferToFill.setSize(1,                 // channel
            bufferSize,        // num samples
            false,             // keepExistingContent
            true,              // clear extra space
            true);             // avoid reallocating
        audioBufferFifo.prepare(1, bufferSize);
        fifoIndex = 0;
        prepared.set(true);
    }
    //======================================================
    int getNumCompleteBuffersAvailable() const { return audioBufferFifo.getNumAvailableForReading(); }
    bool isPrepared() const { return prepared.get(); }
    int getSize() const { return size.get(); }
    //======================================================
    bool getAudioBuffer(BlockType& buf) { return audioBufferFifo.pull(buf); }
private:
    Channel channelToUse;
    int fifoIndex = 0;
    Fifo<BlockType> audioBufferFifo;
    BlockType bufferToFill;
    juce::Atomic<bool> prepared = false;
    juce::Atomic<int> size = 0;

    void pushNextSampleIntoFifo(float sample)
    {
        if (fifoIndex == bufferToFill.getNumSamples())
        {
            auto ok = audioBufferFifo.push(bufferToFill);

            juce::ignoreUnused(ok);

            fifoIndex = 0;
        }

        bufferToFill.setSample(0, fifoIndex, sample);
        ++fifoIndex;
    }
};





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
    // Creating helpful alias and instances of fifo that's declared on top of .h file   ~A
    using BlockType = juce::AudioBuffer<float>;
    SingleChannelSampleFifo<BlockType> leftChannelFifo{ Channel::Left };
    SingleChannelSampleFifo<BlockType> rightChannelFifo{ Channel::Right };

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
