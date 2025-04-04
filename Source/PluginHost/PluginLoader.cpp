#include "PluginLoader.h"

PluginLoader::PluginLoader()
{
    formatManager = std::make_unique<juce::AudioPluginFormatManager>();
    formatManager->addDefaultFormats();
}

PluginLoader::~PluginLoader()
{
    formatManager = nullptr;
}

std::unique_ptr<juce::AudioPluginInstance> PluginLoader::loadPlugin(const juce::String& filePath)
{
    lastErrorMessage = ""; // Clear previous error
    
    if (filePath.isEmpty())
    {
        lastErrorMessage = "Empty file path provided";
        DBG(lastErrorMessage);
        return nullptr;
    }

    DBG("Loading plugin: " + filePath);
    
    // Find the AudioUnit format
    juce::AudioPluginFormat* auFormat = nullptr;
    for (auto format : formatManager->getFormats())
    {
        if (format->getName() == "AudioUnit")
        {
            auFormat = format;
            break;
        }
    }
    
    if (!auFormat)
    {
        lastErrorMessage = "AudioUnit format not available!";
        DBG(lastErrorMessage);
        return nullptr;
    }

    // Find component file
    auto file = juce::File(filePath);
    if (!file.exists())
    {
        // Try searching in standard paths first
        auto searchPaths = getAudioUnitSearchPaths(filePath);
        
        for (const auto& possiblePath : searchPaths)
        {
            if (possiblePath.exists())
            {
                file = possiblePath;
                DBG("Found component at: " + file.getFullPathName());
                break;
            }
        }
    }

    if (!file.exists())
    {
        lastErrorMessage = "Component file not found: " + filePath;
        DBG(lastErrorMessage);
        return nullptr;
    }

    // Create instance
    juce::String errorMessage;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    auFormat->findAllTypesForFile(descriptions, file.getFullPathName());

    if (descriptions.isEmpty())
    {
        lastErrorMessage = "No plugin descriptions found in component";
        DBG(lastErrorMessage);
        return nullptr;
    }

    auto instance = auFormat->createInstanceFromDescription(*descriptions[0], 44100.0, 512, errorMessage);
    if (!instance)
    {
        lastErrorMessage = "Failed to create instance: " + errorMessage;
        DBG(lastErrorMessage);
        return nullptr;
    }

    DBG("Successfully created plugin instance");
    return instance;
}

juce::Array<juce::File> PluginLoader::getAudioUnitSearchPaths(const juce::String& componentName)
{
    juce::Array<juce::File> paths;
    
    // Helper to add paths with different extensions
    auto addPathWithExtensions = [&paths](const juce::File& base, const juce::String& name)
    {
        paths.add(base.getChildFile(name));
        if (!name.endsWithIgnoreCase(".component"))
        {
            paths.add(base.getChildFile(name + ".component"));
        }
    };
    
    // Add standard AU locations
    auto userHome = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    auto systemComponents = juce::File("/Library/Audio/Plug-Ins/Components");
    auto userComponents = userHome.getChildFile("Library/Audio/Plug-Ins/Components");
    
    addPathWithExtensions(systemComponents, componentName);
    addPathWithExtensions(userComponents, componentName);
    
    return paths;
}

juce::String PluginLoader::getExecutableName()
{
    auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto appBundle = executableFile.getParentDirectory()
                                 .getParentDirectory()
                                 .getParentDirectory();
    
    // If we're in a .app bundle, use its name
    if (appBundle.getFileExtension() == ".app")
    {
        return appBundle.getFileNameWithoutExtension();
    }
    
    // Fallback to executable name
    return executableFile.getFileNameWithoutExtension();
}
