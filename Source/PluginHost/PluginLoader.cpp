#include "PluginLoader.h"

#if JUCE_MAC
#include <AudioUnit/AudioUnit.h>
#include <CoreFoundation/CoreFoundation.h>
#include <AudioToolbox/AudioComponent.h>
#endif

// Static log file
juce::File PluginLoader::logFile;

// Initialize log file
juce::File PluginLoader::getLogFile()
{
    if (logFile == juce::File())
    {
        auto appFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
        auto appBundle = appFile.getParentDirectory().getParentDirectory().getParentDirectory();
        auto appDirectory = appBundle.getParentDirectory();
        
        // Create log file next to the app bundle
        logFile = appDirectory.getChildFile(appBundle.getFileNameWithoutExtension() + "_log.txt");
        
        // Clear previous log and write header
        logFile.deleteFile();
        logFile.appendText("=== TowelHost Debug Log ===\n");
        logFile.appendText("Time: " + juce::Time::getCurrentTime().toString(true, true) + "\n");
        logFile.appendText("App: " + appBundle.getFullPathName() + "\n\n");
    }
    return logFile;
}

// Log to both console and file
void PluginLoader::LOG(const juce::String& message)
{
    DBG(message);  // Console output
    
    auto file = getLogFile();
    file.appendText(message + "\n");
}

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
        LOG(lastErrorMessage);
        return nullptr;
    }

    LOG("Loading plugin: " + filePath);
    
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
        LOG(lastErrorMessage);
        return nullptr;
    }

    // SAVIHOST-style: Look for component in same directory as app first
    // Get the directory containing this app bundle
    auto appFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    auto appBundle = appFile.getParentDirectory().getParentDirectory().getParentDirectory();
    auto appDirectory = appBundle.getParentDirectory();
    
    auto file = juce::File(filePath);
    
    LOG("=== SAVIHOST-style Plugin Search ===");
    LOG("Executable: " + appFile.getFullPathName());
    LOG("App bundle: " + appBundle.getFullPathName());
    LOG("App directory: " + appDirectory.getFullPathName());
    LOG("Looking for plugin: " + filePath);
    
    // Priority 1: Look in the SAME directory as the app bundle
    if (!file.exists())
    {
        LOG("\n[Priority 1] Searching same directory as app...");
        
        // Try exact name first
        auto localFile = appDirectory.getChildFile(filePath);
        LOG("  Checking: " + localFile.getFullPathName());
        if (localFile.exists())
        {
            file = localFile;     
            LOG("  ✓ FOUND!");
        }
        else
        {
            LOG("  ✗ Not found");
            
            // Try with .component extension
            localFile = appDirectory.getChildFile(filePath + ".component");
            LOG("  Checking: " + localFile.getFullPathName());
            if (localFile.exists())
            {
                file = localFile;
                LOG("  ✓ FOUND!");
            }
            else
            {
                LOG("  ✗ Not found");
                
                // Try in a subfolder with the same name
                localFile = appDirectory.getChildFile(filePath).getChildFile(filePath + ".component");
                LOG("  Checking subfolder: " + localFile.getFullPathName());
                if (localFile.exists())
                {
                    file = localFile;
                    LOG("  ✓ FOUND in subfolder!");
                }
                else
                {
                    LOG("  ✗ Not found in subfolder either");
                }
            }
        }
    }
    else
    {
        LOG("File path provided is absolute: " + file.getFullPathName());
    }
    
    // Priority 2: Fall back to system AU locations
    if (!file.exists())
    {
        LOG("\n[Priority 2] Searching system AU locations...");
        auto searchPaths = getAudioUnitSearchPaths(filePath);
        
        for (const auto& possiblePath : searchPaths)
        {
            LOG("  Checking: " + possiblePath.getFullPathName());
            if (possiblePath.exists())
            {
                file = possiblePath;
                LOG("  ✓ FOUND!");
                break;
            }
            else
            {
                LOG("  ✗ Not found");
            }
        }
    }

    if (!file.exists())
    {
        lastErrorMessage = "Component file not found: " + filePath + ".component\n\n" +
                          "Looked in:\n" +
                          "1. Same folder as app: " + appDirectory.getFullPathName() + "\n" +
                          "2. ~/Library/Audio/Plug-Ins/Components\n" +
                          "3. /Library/Audio/Plug-Ins/Components\n\n" +
                          "Place the .component file next to the renamed app (SAVIHOST-style).";
        LOG(lastErrorMessage);
        LOG("\n=== Log file location: " + getLogFile().getFullPathName() + " ===");
        return nullptr;
    }
    
    LOG("\n=== Using Component ===");
    LOG("Path: " + file.getFullPathName());

    // Check if it's actually a bundle (component files are bundles/directories)
    if (!file.isDirectory())
    {
        lastErrorMessage = "Component file is not a valid bundle (not a directory): " + file.getFullPathName();
        LOG("ERROR: " + lastErrorMessage);
        return nullptr;
    }
    LOG("✓ Is a bundle/directory");
    
    // Check basic readability by trying to list contents
    int childCount = file.getNumberOfChildFiles(juce::File::findFilesAndDirectories);
    LOG("Bundle contains " + juce::String(childCount) + " items");
    if (childCount == 0)
    {
        LOG("WARNING: Component bundle appears empty or not readable");
        // Don't return - some valid components might appear empty to basic check
    }

    LOG("\n=== Scanning Component ===");
    LOG("Asking JUCE AudioUnit format to scan: " + file.getFullPathName());

    // Create instance with comprehensive error handling
    juce::String errorMessage;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    
    // Direct scan of the specific component file
    LOG("Scanning specific component file...");
    auFormat->findAllTypesForFile(descriptions, file.getFullPathName());
    LOG("Found " + juce::String(descriptions.size()) + " description(s)");
    
    // If JUCE can't find it, try direct instantiation with a minimal description
    if (descriptions.isEmpty())
    {
        LOG("JUCE scan failed. Attempting direct instantiation...");
        
        // Create a minimal plugin description using the file path
        auto* pluginDesc = new juce::PluginDescription();
        pluginDesc->pluginFormatName = "AudioUnit";
        pluginDesc->fileOrIdentifier = file.getFullPathName();
        pluginDesc->name = filePath;
        pluginDesc->manufacturerName = "Unknown";
        pluginDesc->category = "Unknown";
        pluginDesc->version = "1.0";
        pluginDesc->numInputChannels = 2;
        pluginDesc->numOutputChannels = 2;
        pluginDesc->isInstrument = false;
        
        // Try to instantiate directly - this will force JUCE/macOS to load and validate it
        LOG("Trying to instantiate with path-only description...");
        auto directInstance = auFormat->createInstanceFromDescription(*pluginDesc, 44100.0, 512, errorMessage);
        
        if (directInstance)
        {
            LOG("✓ Direct instantiation succeeded!");
            return directInstance;
        }
        else
        {
            LOG("✗ Direct instantiation failed: " + errorMessage);
            
            // Try as instrument
            pluginDesc->isInstrument = true;
            pluginDesc->numInputChannels = 0;
            LOG("Trying as instrument...");
            directInstance = auFormat->createInstanceFromDescription(*pluginDesc, 44100.0, 512, errorMessage);
            
            if (directInstance)
            {
                LOG("✓ Direct instantiation as instrument succeeded!");
                return directInstance;
            }
            else
            {
                LOG("✗ Direct instantiation as instrument also failed: " + errorMessage);
            }
        }
        
        delete pluginDesc;
    }

    if (descriptions.isEmpty())
    {
        lastErrorMessage = "No valid Audio Unit found in component.\n\n"
                          "Possible causes:\n"
                          "• Plugin is not signed/notarized (macOS security) ← MOST LIKELY\n"
                          "• Plugin is not compatible with this architecture\n"
                          "• Plugin failed AU validation\n"
                          "• Component bundle is corrupted\n\n"
                          "QUICK FIX - Run this command:\n"
                          "sudo xattr -r -d com.apple.quarantine \"" + file.getFullPathName() + "\"\n\n"
                          "Other options:\n"
                          "1. Right-click the .component file and select 'Open'\n"
                          "2. Allow in System Settings > Privacy & Security\n"
                          "3. Run: auval -a (to validate all AUs)\n"
                          "4. Check Console.app for detailed error messages";
        LOG(lastErrorMessage);
        LOG("\n=== Log file location: " + getLogFile().getFullPathName() + " ===");
        return nullptr;
    }

    // Log all found descriptions for debugging
    LOG("Found " + juce::String(descriptions.size()) + " plugin description(s) in component");
    for (int i = 0; i < descriptions.size(); ++i)
    {
        auto* desc = descriptions[i];
        LOG("  [" + juce::String(i) + "] " + desc->name + 
            " (type: " + desc->pluginFormatName + ")" +
            " (manufacturer: " + desc->manufacturerName + ")" +
            " (version: " + desc->version + ")" +
            " (ID: " + desc->createIdentifierString() + ")");
    }

    // Get the current audio device settings
    double sampleRate = 44100.0;
    int blockSize = 512;
    
    // Try to instantiate each description until one succeeds
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::String lastAttemptError;
    
    for (int i = 0; i < descriptions.size(); ++i)
    {
        auto* desc = descriptions[i];
        
        // Validate the description
        if (desc->name.isEmpty())
        {
            LOG("  [" + juce::String(i) + "] Skipping: Empty name");
            continue;
        }
        
        if (desc->pluginFormatName != "AudioUnit")
        {
            LOG("  [" + juce::String(i) + "] Skipping: Not an AudioUnit (" + desc->pluginFormatName + ")");
            continue;
        }
        
        LOG("Attempting to instantiate [" + juce::String(i) + "]: " + desc->name);
        errorMessage.clear();
        
        instance = auFormat->createInstanceFromDescription(*desc, sampleRate, blockSize, errorMessage);
        
        if (instance)
        {
            LOG("✓ Successfully created plugin instance: " + instance->getName());
            return instance;
        }
        else
        {
            lastAttemptError = errorMessage;
            LOG("  ✗ Failed to instantiate [" + juce::String(i) + "]: " + errorMessage);
        }
    }
    
    // All attempts failed
    lastErrorMessage = "Failed to create any plugin instance from component.\n";
    if (lastAttemptError.isNotEmpty())
        lastErrorMessage += "Last error: " + lastAttemptError;
    else
        lastErrorMessage += "No error message provided by plugin format.";
    
    LOG(lastErrorMessage);
    LOG("\n=== Log file location: " + getLogFile().getFullPathName() + " ===");
    return nullptr;
}

juce::Array<juce::File> PluginLoader::getAudioUnitSearchPaths(const juce::String& componentName)
{
    juce::Array<juce::File> paths;
    
    // Standard AU locations
    auto userHome = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    auto systemComponents = juce::File("/Library/Audio/Plug-Ins/Components");
    auto userComponents = userHome.getChildFile("Library/Audio/Plug-Ins/Components");
    
    // Helper to add paths with different extensions
    auto addPathWithExtensions = [&paths](const juce::File& base, const juce::String& name)
    {
        // Try exact name
        paths.add(base.getChildFile(name));
        
        // Try with .component extension if not already present
        if (!name.endsWithIgnoreCase(".component"))
        {
            paths.add(base.getChildFile(name + ".component"));
        }
        
        // Try case variations (some plugins have inconsistent naming)
        auto lowerName = name.toLowerCase();
        if (lowerName != name)
        {
            paths.add(base.getChildFile(lowerName));
            if (!lowerName.endsWithIgnoreCase(".component"))
                paths.add(base.getChildFile(lowerName + ".component"));
        }
    };
    
    // Priority order: user components first (often more up-to-date)
    addPathWithExtensions(userComponents, componentName);
    addPathWithExtensions(systemComponents, componentName);
    
    // Also try scanning the directories for case-insensitive matches
    // This handles cases where the app name doesn't exactly match the component name
    auto scanForMatch = [&componentName](const juce::File& directory) -> juce::File
    {
        if (!directory.exists() || !directory.isDirectory())
            return {};
            
        juce::String searchName = componentName;
        if (searchName.endsWithIgnoreCase(".component"))
            searchName = searchName.dropLastCharacters(10); // Remove .component
            
        auto files = directory.findChildFiles(juce::File::findDirectories, false, "*.component");
        for (const auto& f : files)
        {
            auto fileName = f.getFileNameWithoutExtension();
            if (fileName.equalsIgnoreCase(searchName))
                return f;
        }
        return {};
    };
    
    // Add case-insensitive directory scan results
    auto userMatch = scanForMatch(userComponents);
    if (userMatch.exists())
        paths.add(userMatch);
        
    auto systemMatch = scanForMatch(systemComponents);
    if (systemMatch.exists())
        paths.add(systemMatch);
    
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
