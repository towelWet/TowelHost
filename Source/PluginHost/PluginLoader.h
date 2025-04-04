#pragma once
#include <JuceHeader.h>

class PluginLoader
{
public:
    PluginLoader();
    ~PluginLoader();
    
    std::unique_ptr<juce::AudioPluginInstance> loadPlugin(const juce::String& filePath);
    static juce::String getExecutableName();
    juce::String getLastError() const { return lastErrorMessage; }
    
private:
    std::unique_ptr<juce::AudioPluginFormatManager> formatManager;
    juce::Array<juce::File> getAudioUnitSearchPaths(const juce::String& componentName);
    juce::String lastErrorMessage;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginLoader)
};
