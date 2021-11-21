/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EQ_LiteAudioProcessor::EQ_LiteAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

EQ_LiteAudioProcessor::~EQ_LiteAudioProcessor()
{
}

//==============================================================================
const juce::String EQ_LiteAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EQ_LiteAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EQ_LiteAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EQ_LiteAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EQ_LiteAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EQ_LiteAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EQ_LiteAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EQ_LiteAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EQ_LiteAudioProcessor::getProgramName (int index)
{
    return {};
}

void EQ_LiteAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EQ_LiteAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    // Creating a specs object that will be passed to each element of the signal chain and then setting its values  ~A

    juce::dsp::ProcessSpec specs;
    specs.maximumBlockSize = samplesPerBlock;
    specs.numChannels = 1;
    specs.sampleRate = sampleRate;
    
    leftChain.prepare(specs);
    rightChain.prepare(specs);

    // Creating audio processing coefficients for peak filters/bands   ~A
    auto chainSettings = getChainSettings(apvts);

    auto band1Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                                                chainSettings.band1Freq,
                                                                                chainSettings.band1Quality,
                                                                                juce::Decibels::decibelsToGain(chainSettings.band1GainDB));
    *leftChain.get<ChainPositions::Band1>().coefficients = *band1Coefficients;
    *rightChain.get<ChainPositions::Band1>().coefficients = *band1Coefficients;

    auto band2Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                                                 chainSettings.band2Freq,
                                                                                 chainSettings.band2Quality,
                                                                                 juce::Decibels::decibelsToGain(chainSettings.band2GainDB));
    *leftChain.get<ChainPositions::Band2>().coefficients = *band2Coefficients;
    *rightChain.get<ChainPositions::Band2>().coefficients = *band2Coefficients;

    auto band3Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                                                 chainSettings.band3Freq,
                                                                                 chainSettings.band3Quality,
                                                                                 juce::Decibels::decibelsToGain(chainSettings.band3GainDB));
    *leftChain.get<ChainPositions::Band3>().coefficients = *band3Coefficients;
    *rightChain.get<ChainPositions::Band3>().coefficients = *band3Coefficients;

    // Creating audio processing coefficients for low and high pass filters   ~A

    // Low cut (high pass)
    auto LcutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq, 
                                                                                                       sampleRate,
                                                                                                       2*(chainSettings.lowCutSlope+1)); 
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    leftLowCut.setBypassed<0>(true);     //  Initially bypassing all elements in the chain  ~A
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    switch (chainSettings.lowCutSlope)
    {
        case Slope_12:
        {
            *leftLowCut.get<0>().coefficients = *LcutCoefficients[0];
            leftLowCut.setBypassed<0>(false);
            break;
        }

        case Slope_24:
        {
            *leftLowCut.get<0>().coefficients = *LcutCoefficients[0];    // In case of 24 dB/Oct slope the helper function returns an arr with 2 coeff objects  ~A
            leftLowCut.setBypassed<0>(false);
            *leftLowCut.get<1>().coefficients = *LcutCoefficients[1];
            leftLowCut.setBypassed<1>(false);
            break;
        }

        case Slope_36:
        {
            *leftLowCut.get<0>().coefficients = *LcutCoefficients[0];    // And so on    ~A
            leftLowCut.setBypassed<0>(false);
            *leftLowCut.get<1>().coefficients = *LcutCoefficients[1];
            leftLowCut.setBypassed<1>(false);
            *leftLowCut.get<2>().coefficients = *LcutCoefficients[2];
            leftLowCut.setBypassed<2>(false);
            break;
        }

        case Slope_48:
        {
            *leftLowCut.get<0>().coefficients = *LcutCoefficients[0];    
            leftLowCut.setBypassed<0>(false);
            *leftLowCut.get<1>().coefficients = *LcutCoefficients[1];
            leftLowCut.setBypassed<1>(false);
            *leftLowCut.get<2>().coefficients = *LcutCoefficients[2];
            leftLowCut.setBypassed<2>(false);
            *leftLowCut.get<3>().coefficients = *LcutCoefficients[3];
            leftLowCut.setBypassed<3>(false);
            break;
        }

 
    }
    
    // Repeating the process for the right channel      ~A
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    rightLowCut.setBypassed<0>(true);     
    rightLowCut.setBypassed<1>(true);
    rightLowCut.setBypassed<2>(true);
    rightLowCut.setBypassed<3>(true);

    switch (chainSettings.lowCutSlope)
    {
        case Slope_12:
        {
            *rightLowCut.get<0>().coefficients = *LcutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
            break;
        }

        case Slope_24:
        {
             *rightLowCut.get<0>().coefficients = *LcutCoefficients[0];
             rightLowCut.setBypassed<0>(false);
             *rightLowCut.get<1>().coefficients = *LcutCoefficients[1];
             rightLowCut.setBypassed<1>(false);
             break;
        }

        case Slope_36:
        {
            *rightLowCut.get<0>().coefficients = *LcutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
            *rightLowCut.get<1>().coefficients = *LcutCoefficients[1];
            rightLowCut.setBypassed<1>(false);
            *rightLowCut.get<2>().coefficients = *LcutCoefficients[2];
            rightLowCut.setBypassed<2>(false);
            break;
        }      

        case Slope_48:
        {
            *rightLowCut.get<0>().coefficients = *LcutCoefficients[0];
            rightLowCut.setBypassed<0>(false);
            *rightLowCut.get<1>().coefficients = *LcutCoefficients[1];
            rightLowCut.setBypassed<1>(false);
            *rightLowCut.get<2>().coefficients = *LcutCoefficients[2];
            rightLowCut.setBypassed<2>(false);
            *rightLowCut.get<3>().coefficients = *LcutCoefficients[3];
            rightLowCut.setBypassed<3>(false);
            break;
        }

    }





    // High cut (low pass)
    auto HcutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                                       sampleRate,
                                                                                                       2 * (chainSettings.highCutSlope + 1));
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    leftHighCut.setBypassed<0>(true);     //  Initially bypassing all elements in the chain     ~A
    leftHighCut.setBypassed<1>(true);
    leftHighCut.setBypassed<2>(true);
    leftHighCut.setBypassed<3>(true);

    switch (chainSettings.highCutSlope)
    {
        case Slope_12:
        {
            *leftHighCut.get<0>().coefficients = *HcutCoefficients[0];
            leftHighCut.setBypassed<0>(false);
            break;
        }

        case Slope_24:
        {
            *leftHighCut.get<0>().coefficients = *HcutCoefficients[0];    // In case of 24 dB/Oct slope the helper function returns an arr with 2 coeff objects  ~A
            leftHighCut.setBypassed<0>(false);
            *leftHighCut.get<1>().coefficients = *HcutCoefficients[1];
            leftHighCut.setBypassed<1>(false);
            break;
        }

    case Slope_36:
        {
            *leftHighCut.get<0>().coefficients = *HcutCoefficients[0];    // And so on    ~A
            leftHighCut.setBypassed<0>(false);
            *leftHighCut.get<1>().coefficients = *HcutCoefficients[1];
            leftHighCut.setBypassed<1>(false);
            *leftHighCut.get<2>().coefficients = *HcutCoefficients[2];
            leftHighCut.setBypassed<2>(false);
            break;
        }

    case Slope_48:
        {
            *leftHighCut.get<0>().coefficients = *HcutCoefficients[0];
            leftHighCut.setBypassed<0>(false);
            *leftHighCut.get<1>().coefficients = *HcutCoefficients[1];
            leftHighCut.setBypassed<1>(false);
            *leftHighCut.get<2>().coefficients = *HcutCoefficients[2];
            leftHighCut.setBypassed<2>(false);
            *leftHighCut.get<3>().coefficients = *HcutCoefficients[3];
            leftHighCut.setBypassed<3>(false);
        break;
        }

    }

    // Repeating the process for the right channel      ~A
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    rightHighCut.setBypassed<0>(true);     //  Initially bypassing all elements in the chain     ~A
    rightHighCut.setBypassed<1>(true);
    rightHighCut.setBypassed<2>(true);
    rightHighCut.setBypassed<3>(true);

    switch (chainSettings.highCutSlope)
    {
        case Slope_12:
        {
            *rightHighCut.get<0>().coefficients = *HcutCoefficients[0];
            rightHighCut.setBypassed<0>(false);
            break;
        }

        case Slope_24:
        {
            *rightHighCut.get<0>().coefficients = *HcutCoefficients[0];    // In case of 24 dB/Oct slope the helper function returns an arr with 2 coeff objects  ~A
            rightHighCut.setBypassed<0>(false);
            *rightHighCut.get<1>().coefficients = *HcutCoefficients[1];
            rightHighCut.setBypassed<1>(false);
            break;
        }

        case Slope_36:
        {
            *rightHighCut.get<0>().coefficients = *HcutCoefficients[0];    // And so on    ~A
            rightHighCut.setBypassed<0>(false);
            *rightHighCut.get<1>().coefficients = *HcutCoefficients[1];
            rightHighCut.setBypassed<1>(false);
            *rightHighCut.get<2>().coefficients = *HcutCoefficients[2];
            rightHighCut.setBypassed<2>(false);
            break;
        }

        case Slope_48:
        {
            *rightHighCut.get<0>().coefficients = *HcutCoefficients[0];
            rightHighCut.setBypassed<0>(false);
            *rightHighCut.get<1>().coefficients = *HcutCoefficients[1];
            rightHighCut.setBypassed<1>(false);
            *rightHighCut.get<2>().coefficients = *HcutCoefficients[2];
            rightHighCut.setBypassed<2>(false);
            *rightHighCut.get<3>().coefficients = *HcutCoefficients[3];
            rightHighCut.setBypassed<3>(false);
            break;
        }


    }
}

void EQ_LiteAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EQ_LiteAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void EQ_LiteAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Updating the parameters before processing the audio     ~A
    
    // 3 peak bands parameters

    auto chainSettings = getChainSettings(apvts);

    auto band1Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                                                 chainSettings.band1Freq,
                                                                                 chainSettings.band1Quality,
                                                                                 juce::Decibels::decibelsToGain(chainSettings.band1GainDB));
    *leftChain.get<ChainPositions::Band1>().coefficients = *band1Coefficients;
    *rightChain.get<ChainPositions::Band1>().coefficients = *band1Coefficients;

    auto band2Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                                                 chainSettings.band2Freq,
                                                                                 chainSettings.band2Quality,
                                                                                 juce::Decibels::decibelsToGain(chainSettings.band2GainDB));
    *leftChain.get<ChainPositions::Band2>().coefficients = *band2Coefficients;
    *rightChain.get<ChainPositions::Band2>().coefficients = *band2Coefficients;

    auto band3Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                                                 chainSettings.band3Freq,
                                                                                 chainSettings.band3Quality,
                                                                                 juce::Decibels::decibelsToGain(chainSettings.band3GainDB));
    *leftChain.get<ChainPositions::Band3>().coefficients = *band3Coefficients;
    *rightChain.get<ChainPositions::Band3>().coefficients = *band3Coefficients;



    // Low cut filter parameters
    auto LcutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq,
                                                                                                       getSampleRate(),
                                                                                                       2 * (chainSettings.lowCutSlope + 1));
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    leftLowCut.setBypassed<0>(true);     //  Bypassing elements of the chain that aren't the low cut filter  ~A
    leftLowCut.setBypassed<1>(true);
    leftLowCut.setBypassed<2>(true);
    leftLowCut.setBypassed<3>(true);

    switch (chainSettings.lowCutSlope)
    {
    case Slope_12:
    {
        *leftLowCut.get<0>().coefficients = *LcutCoefficients[0];
        leftLowCut.setBypassed<0>(false);
        break;
    }

    case Slope_24:
    {
        *leftLowCut.get<0>().coefficients = *LcutCoefficients[0];    // In case of 24 dB/Oct slope the helper function returns an arr with 2 coeff objects  ~A
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *LcutCoefficients[1];
        leftLowCut.setBypassed<1>(false);
        break;
    }

    case Slope_36:
    {
        *leftLowCut.get<0>().coefficients = *LcutCoefficients[0];    // And so on    ~A
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *LcutCoefficients[1];
        leftLowCut.setBypassed<1>(false);
        *leftLowCut.get<2>().coefficients = *LcutCoefficients[2];
        leftLowCut.setBypassed<2>(false);
        break;
    }

    case Slope_48:
    {
        *leftLowCut.get<0>().coefficients = *LcutCoefficients[0];
        leftLowCut.setBypassed<0>(false);
        *leftLowCut.get<1>().coefficients = *LcutCoefficients[1];
        leftLowCut.setBypassed<1>(false);
        *leftLowCut.get<2>().coefficients = *LcutCoefficients[2];
        leftLowCut.setBypassed<2>(false);
        *leftLowCut.get<3>().coefficients = *LcutCoefficients[3];
        leftLowCut.setBypassed<3>(false);
        break;
    }


    }

    // Repeating the process for the right channel      ~A
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    rightLowCut.setBypassed<0>(true);
    rightLowCut.setBypassed<1>(true);
    rightLowCut.setBypassed<2>(true);
    rightLowCut.setBypassed<3>(true);

    switch (chainSettings.lowCutSlope)
    {
    case Slope_12:
    {
        *rightLowCut.get<0>().coefficients = *LcutCoefficients[0];
        rightLowCut.setBypassed<0>(false);
        break;
    }

    case Slope_24:
    {
        *rightLowCut.get<0>().coefficients = *LcutCoefficients[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *LcutCoefficients[1];
        rightLowCut.setBypassed<1>(false);
        break;
    }

    case Slope_36:
    {
        *rightLowCut.get<0>().coefficients = *LcutCoefficients[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *LcutCoefficients[1];
        rightLowCut.setBypassed<1>(false);
        *rightLowCut.get<2>().coefficients = *LcutCoefficients[2];
        rightLowCut.setBypassed<2>(false);
        break;
    }

    case Slope_48:
    {
        *rightLowCut.get<0>().coefficients = *LcutCoefficients[0];
        rightLowCut.setBypassed<0>(false);
        *rightLowCut.get<1>().coefficients = *LcutCoefficients[1];
        rightLowCut.setBypassed<1>(false);
        *rightLowCut.get<2>().coefficients = *LcutCoefficients[2];
        rightLowCut.setBypassed<2>(false);
        *rightLowCut.get<3>().coefficients = *LcutCoefficients[3];
        rightLowCut.setBypassed<3>(false);
        break;
    }

    }


    // High cut (low pass)
    auto HcutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                                       getSampleRate(),
                                                                                                       2 * (chainSettings.highCutSlope + 1));
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    leftHighCut.setBypassed<0>(true);     //  Initially bypassing all elements in the chain     ~A
    leftHighCut.setBypassed<1>(true);
    leftHighCut.setBypassed<2>(true);
    leftHighCut.setBypassed<3>(true);

    switch (chainSettings.highCutSlope)
    {
    case Slope_12:
    {
        *leftHighCut.get<0>().coefficients = *HcutCoefficients[0];
        leftHighCut.setBypassed<0>(false);
        break;
    }

    case Slope_24:
    {
        *leftHighCut.get<0>().coefficients = *HcutCoefficients[0];    // In case of 24 dB/Oct slope the helper function returns an arr with 2 coeff objects  ~A
        leftHighCut.setBypassed<0>(false);
        *leftHighCut.get<1>().coefficients = *HcutCoefficients[1];
        leftHighCut.setBypassed<1>(false);
        break;
    }

    case Slope_36:
    {
        *leftHighCut.get<0>().coefficients = *HcutCoefficients[0];    // And so on    ~A
        leftHighCut.setBypassed<0>(false);
        *leftHighCut.get<1>().coefficients = *HcutCoefficients[1];
        leftHighCut.setBypassed<1>(false);
        *leftHighCut.get<2>().coefficients = *HcutCoefficients[2];
        leftHighCut.setBypassed<2>(false);
        break;
    }

    case Slope_48:
    {
        *leftHighCut.get<0>().coefficients = *HcutCoefficients[0];
        leftHighCut.setBypassed<0>(false);
        *leftHighCut.get<1>().coefficients = *HcutCoefficients[1];
        leftHighCut.setBypassed<1>(false);
        *leftHighCut.get<2>().coefficients = *HcutCoefficients[2];
        leftHighCut.setBypassed<2>(false);
        *leftHighCut.get<3>().coefficients = *HcutCoefficients[3];
        leftHighCut.setBypassed<3>(false);
        break;
    }

    }

    // Repeating the process for the right channel      ~A
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    rightHighCut.setBypassed<0>(true);     //  Initially bypassing all elements in the chain     ~A
    rightHighCut.setBypassed<1>(true);
    rightHighCut.setBypassed<2>(true);
    rightHighCut.setBypassed<3>(true);

    switch (chainSettings.highCutSlope)
    {
    case Slope_12:
    {
        *rightHighCut.get<0>().coefficients = *HcutCoefficients[0];
        rightHighCut.setBypassed<0>(false);
        break;
    }

    case Slope_24:
    {
        *rightHighCut.get<0>().coefficients = *HcutCoefficients[0];    // In case of 24 dB/Oct slope the helper function returns an arr with 2 coeff objects  ~A
        rightHighCut.setBypassed<0>(false);
        *rightHighCut.get<1>().coefficients = *HcutCoefficients[1];
        rightHighCut.setBypassed<1>(false);
        break;
    }

    case Slope_36:
    {
        *rightHighCut.get<0>().coefficients = *HcutCoefficients[0];    // And so on    ~A
        rightHighCut.setBypassed<0>(false);
        *rightHighCut.get<1>().coefficients = *HcutCoefficients[1];
        rightHighCut.setBypassed<1>(false);
        *rightHighCut.get<2>().coefficients = *HcutCoefficients[2];
        rightHighCut.setBypassed<2>(false);
        break;
    }

    case Slope_48:
    {
        *rightHighCut.get<0>().coefficients = *HcutCoefficients[0];
        rightHighCut.setBypassed<0>(false);
        *rightHighCut.get<1>().coefficients = *HcutCoefficients[1];
        rightHighCut.setBypassed<1>(false);
        *rightHighCut.get<2>().coefficients = *HcutCoefficients[2];
        rightHighCut.setBypassed<2>(false);
        *rightHighCut.get<3>().coefficients = *HcutCoefficients[3];
        rightHighCut.setBypassed<3>(false);
        break;
    }


    }





    // Creating context to be passed to the chain       ~A
    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool EQ_LiteAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EQ_LiteAudioProcessor::createEditor()
{
    //return new EQ_LiteAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void EQ_LiteAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EQ_LiteAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}


chainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    chainSettings chSettings;

    chSettings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    chSettings.highCutFreq = apvts.getRawParameterValue("HiCut Freq")->load();
    chSettings.band1Freq = apvts.getRawParameterValue("Band1 Freq")->load();
    chSettings.band1GainDB = apvts.getRawParameterValue("Band1 Gain")->load();
    chSettings.band1Quality = apvts.getRawParameterValue("Band1 Quality")->load();
    chSettings.band2Freq = apvts.getRawParameterValue("Band2 Freq")->load();
    chSettings.band2GainDB = apvts.getRawParameterValue("Band2 Gain")->load();
    chSettings.band2Quality = apvts.getRawParameterValue("Band2 Quality")->load();
    chSettings.band3Freq = apvts.getRawParameterValue("Band3 Freq")->load();
    chSettings.band3GainDB = apvts.getRawParameterValue("Band3 Gain")->load();
    chSettings.band3Quality = apvts.getRawParameterValue("Band3 Quality")->load();
    chSettings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());    // Static casting to slope enum type not to interfere   ~A
    chSettings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HiCut Slope")->load());

    return chSettings;
}


    // Adding audio processor parameters here. Low cut and high cut with steepness choice, 3 bands with Q and dB parameters    ~A
juce::AudioProcessorValueTreeState::ParameterLayout
EQ_LiteAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20.f));  // Setting skew factor to 0.25 for more natural frequence scrolling     ~A

    layout.add(std::make_unique<juce::AudioParameterFloat>("HiCut Freq", "HiCut Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 20000.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Freq", "Band1 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 400.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Gain", "Band1 Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Quality", "Band1 Quality",
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Freq", "Band2 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 1000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Gain", "Band2 Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Quality", "Band2 Quality",
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band3 Freq", "Band3 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.25f), 5000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band3 Gain", "Band3 Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band3 Quality", "Band3 Quality",
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));


    juce::StringArray slopeChoices{"12 dB/Oct", "24 dB/Oct", "36 dB/Oct", "48 dB/Oct"};
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope",
        slopeChoices, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>("HiCut Slope", "HiCut Slope",
        slopeChoices, 0));

    return layout;
}



//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQ_LiteAudioProcessor();
}
