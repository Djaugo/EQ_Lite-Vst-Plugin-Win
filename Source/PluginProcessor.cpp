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

    // Helper function to update all the filters (check its declaration)     ~A

    updateFilters();

    // Preparing the fifos for spectrum analyser    ~A
    leftChannelFifo.prepare(samplesPerBlock);
    rightChannelFifo.prepare(samplesPerBlock);
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

    // Helper function to update all the filters (check its declaration)     ~A

    updateFilters();
    
    // Creating context to be passed to the chain       ~A
    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);

    // Pushing the buffers into fifo    ~A
    leftChannelFifo.update(buffer);
    rightChannelFifo.update(buffer);
}

//==============================================================================
bool EQ_LiteAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EQ_LiteAudioProcessor::createEditor()
{
    // Uncommenting the line that shows no GUI blank editor so I can create a custom one in PluginEditor files  ~A

    return new EQ_LiteAudioProcessorEditor (*this);
    //return new juce::GenericAudioProcessorEditor(*this);

}

//==============================================================================
void EQ_LiteAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.

    // Creating state saving functionality  ~A
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);

}

void EQ_LiteAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.

    // Creating state loading functionality     ~A
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
        updateFilters();
    }
}


ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings chSettings;

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


Coefficients makeBand1Filter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.band1Freq,
                                                               chainSettings.band1Quality,
                                                               juce::Decibels::decibelsToGain(chainSettings.band1GainDB));
}

Coefficients makeBand2Filter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.band2Freq,
                                                               chainSettings.band2Quality,
                                                               juce::Decibels::decibelsToGain(chainSettings.band2GainDB));
}

Coefficients makeBand3Filter(const ChainSettings& chainSettings, double sampleRate)
{
    return juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate,
                                                               chainSettings.band3Freq,
                                                               chainSettings.band3Quality,
                                                               juce::Decibels::decibelsToGain(chainSettings.band3GainDB));
}

// Code cleaning helper functions declarations      ~A
void EQ_LiteAudioProcessor::updateBand1Filter(const ChainSettings& chainSettings)
{
    /*auto band1Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                                                chainSettings.band1Freq,
                                                                                chainSettings.band1Quality,
                                                                                juce::Decibels::decibelsToGain(chainSettings.band1GainDB));*/
    auto band1Coefficients = makeBand1Filter(chainSettings, getSampleRate());
    updateCoefficients(leftChain.get<ChainPositions::Band1>().coefficients, band1Coefficients);
    updateCoefficients(rightChain.get<ChainPositions::Band1>().coefficients, band1Coefficients);
}

void EQ_LiteAudioProcessor::updateBand2Filter(const ChainSettings& chainSettings)
{
   /* auto band2Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                                                 chainSettings.band2Freq,
                                                                                 chainSettings.band2Quality,
                                                                                 juce::Decibels::decibelsToGain(chainSettings.band2GainDB));*/
    auto band2Coefficients = makeBand2Filter(chainSettings, getSampleRate());
    updateCoefficients(leftChain.get<ChainPositions::Band2>().coefficients, band2Coefficients);
    updateCoefficients(rightChain.get<ChainPositions::Band2>().coefficients, band2Coefficients);
}

void EQ_LiteAudioProcessor::updateBand3Filter(const ChainSettings& chainSettings)
{
   /* auto band3Coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),
                                                                                 chainSettings.band3Freq,
                                                                                 chainSettings.band3Quality,
                                                                                 juce::Decibels::decibelsToGain(chainSettings.band3GainDB));*/
    auto band3Coefficients = makeBand3Filter(chainSettings, getSampleRate());
    updateCoefficients(leftChain.get<ChainPositions::Band3>().coefficients, band3Coefficients);
    updateCoefficients(rightChain.get<ChainPositions::Band3>().coefficients, band3Coefficients);
}

void updateCoefficients(Coefficients& old, const Coefficients& replacement)
{
    *old = *replacement;
}

void EQ_LiteAudioProcessor::updateLowCutFilters(const ChainSettings& chainSettings)
{
    // Low cut filter parameters

    auto LcutCoefficients = makeLowCutFilter(chainSettings, getSampleRate());
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    updateCutFilter(leftLowCut, LcutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(rightLowCut, LcutCoefficients, chainSettings.lowCutSlope);
}

void EQ_LiteAudioProcessor::updateHighCutFilters(const ChainSettings& chainSettings)
{
    // High cut (low pass)
    auto HcutCoefficients = makeHighCutFilter(chainSettings, getSampleRate());
    // Repeating the process for both channels      ~A
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    updateCutFilter(leftHighCut, HcutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, HcutCoefficients, chainSettings.highCutSlope);
}

void EQ_LiteAudioProcessor::updateFilters()
{
    auto chainSettings = getChainSettings(apvts);
    updateLowCutFilters(chainSettings);
    updateBand1Filter(chainSettings);
    updateBand2Filter(chainSettings);
    updateBand3Filter(chainSettings);
    updateHighCutFilters(chainSettings);
}


    // Adding audio processor parameters here. Low cut and high cut with steepness choice, 3 bands with Q and dB parameters    ~A
juce::AudioProcessorValueTreeState::ParameterLayout
EQ_LiteAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.3f), 20.f));  // Setting skew factor to 0.25 for more natural frequence scrolling     ~A

    layout.add(std::make_unique<juce::AudioParameterFloat>("HiCut Freq", "HiCut Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.3f), 20000.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Freq", "Band1 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.3f), 400.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Gain", "Band1 Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Quality", "Band1 Quality",
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Freq", "Band2 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.3f), 1000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Gain", "Band2 Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Quality", "Band2 Quality",
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band3 Freq", "Band3 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.3f), 5000.f));

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
