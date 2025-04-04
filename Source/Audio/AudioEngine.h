#pragma once
#include <JuceHeader.h>

class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;
    
    void initialize();
    void start();
    void stop();
    
    void audioDeviceIOCallback(const float** inputChannelData,
                             int numInputChannels,
                             float** outputChannelData,
                             int numOutputChannels,
                             int numSamples) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    
    void setProcessor(juce::AudioProcessor* processor);
    
private:
    std::unique_ptr<juce::AudioDeviceManager> deviceManager;
    juce::AudioProcessor* currentProcessor{nullptr};
    bool isInitialized{false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};
