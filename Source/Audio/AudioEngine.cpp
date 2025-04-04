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
        juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels, numSamples);
        juce::MidiBuffer midiBuffer;
        currentProcessor->processBlock(buffer, midiBuffer);
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
            currentProcessor->setPlayConfigDetails(device->getActiveInputChannels().countNumberOfSetBits(),
                                                device->getActiveOutputChannels().countNumberOfSetBits(),
                                                device->getCurrentSampleRate(),
                                                device->getCurrentBufferSizeSamples());
            currentProcessor->prepareToPlay(device->getCurrentSampleRate(),
                                         device->getCurrentBufferSizeSamples());
        }
    }
}
