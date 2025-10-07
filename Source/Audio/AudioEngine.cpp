#include "AudioEngine.h"

AudioEngine::AudioEngine() : deviceManager(std::make_unique<juce::AudioDeviceManager>()) {}

AudioEngine::~AudioEngine()
{
    stop();
}

void AudioEngine::initialize()
{
    if (!isInitialized)
    {
        deviceManager->initialise(2, 2, nullptr, true);
        isInitialized = true;
    }
}

void AudioEngine::start()
{
    if (isInitialized)
        deviceManager->addAudioCallback(this);
}

void AudioEngine::stop()
{
    deviceManager->removeAudioCallback(this);
}

void AudioEngine::audioDeviceIOCallback(const float** inputChannelData,
                                      int numInputChannels,
                                      float** outputChannelData,
                                      int numOutputChannels,
                                      int numSamples)
{
    if (currentProcessor != nullptr)
    {
        // Create a proper buffer with both input and output channels
        juce::AudioBuffer<float> buffer(juce::jmax(numInputChannels, numOutputChannels), numSamples);
        
        // Clear the buffer first to avoid undefined behavior
        buffer.clear();
        
        // Copy input data to buffer
        for (int i = 0; i < numInputChannels; ++i)
        {
            if (inputChannelData[i] != nullptr && i < buffer.getNumChannels())
                buffer.copyFrom(i, 0, inputChannelData[i], numSamples);
        }
        
        juce::MidiBuffer midiBuffer;
        currentProcessor->processBlock(buffer, midiBuffer);
        
        // Copy processed data to output
        for (int i = 0; i < numOutputChannels; ++i)
        {
            if (outputChannelData[i] != nullptr)
            {
                if (i < buffer.getNumChannels())
                    juce::FloatVectorOperations::copy(outputChannelData[i], buffer.getReadPointer(i), numSamples);
                else
                    juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);
            }
        }
    }
    else
    {
        // Clear output if no processor
        for (int i = 0; i < numOutputChannels; ++i)
        {
            if (outputChannelData[i] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);
        }
    }
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (currentProcessor != nullptr)
        currentProcessor->prepareToPlay(device->getCurrentSampleRate(),
                                      device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    if (currentProcessor != nullptr)
        currentProcessor->releaseResources();
}

void AudioEngine::setProcessor(juce::AudioProcessor* processor)
{
    if (currentProcessor == processor)
        return;
        
    if (currentProcessor != nullptr)
    {
        currentProcessor->releaseResources();
    }
    
    currentProcessor = processor;
    
    if (currentProcessor != nullptr && isInitialized)
    {
        auto* device = deviceManager->getCurrentAudioDevice();
        if (device != nullptr)
        {
            int numInputChannels = device->getActiveInputChannels().countNumberOfSetBits();
            int numOutputChannels = device->getActiveOutputChannels().countNumberOfSetBits();
            double sampleRate = device->getCurrentSampleRate();
            int bufferSize = device->getCurrentBufferSizeSamples();
            
            DBG("Configuring processor: " + juce::String(numInputChannels) + " in, " + 
                juce::String(numOutputChannels) + " out, " + 
                juce::String(sampleRate) + " Hz, " + 
                juce::String(bufferSize) + " samples");
            
            // Verify the processor can handle this configuration
            if (numInputChannels == 0 && numOutputChannels == 0)
            {
                DBG("Warning: No active audio channels!");
                numInputChannels = 2;  // Fallback to stereo
                numOutputChannels = 2;
            }
            
            currentProcessor->setPlayConfigDetails(numInputChannels,
                                                numOutputChannels,
                                                sampleRate,
                                                bufferSize);
            currentProcessor->prepareToPlay(sampleRate, bufferSize);
        }
    }
}
