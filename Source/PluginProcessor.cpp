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

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
    }
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
    // Adding audio processor parameters here. Low cut and high cut with steepness choice, 3 bands with Q and dB parameters    ~A
juce::AudioProcessorValueTreeState::ParameterLayout
EQ_LiteAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq", "LowCut Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f), 20.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("HiCut Freq", "HiCut Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f), 20000.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Freq", "Band1 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f), 400.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Gain", "Band1 Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band1 Quality", "Band1 Quality",
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Freq", "Band2 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f), 1000.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Gain", "Band2 Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.1f, 1.f), 0.f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("Band2 Quality", "Band2 Quality",
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f), 1.f));



    layout.add(std::make_unique<juce::AudioParameterFloat>("Band3 Freq", "Band3 Freq",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f), 5000.f));

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
