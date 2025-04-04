#include <JuceHeader.h>
#include "PluginHost/PluginLoader.h"
#include "Audio/AudioEngine.h"

// Make MainComponent listen to its children (specifically the plugin editor)
class MainComponent : public juce::Component,
                      public juce::ComponentListener // Add listener inheritance
{
public:
    enum class LoadStatus {
        NoPlugin,
        LoadFailed,
        LoadedNoEditor,
        LoadedWithEditor
    };

    // Constructor: Set initial state and try loading based on executable name
    MainComponent()
    {
        // Default size only if no plugin/editor loads
        setSize(400, 200);
        audioEngine.initialize();
        status = LoadStatus::NoPlugin;

        // Use the static method from PluginLoader directly
        auto executableName = PluginLoader::getExecutableName();

        // Skip loading if running as the default "TowelHost" name
        if (!executableName.isEmpty() && !executableName.equalsIgnoreCase("TowelHost"))
        {
            DBG("Running as renamed plugin host for: " + executableName);
            loadPluginFromName(executableName);
        }
        else if (executableName.equalsIgnoreCase("TowelHost"))
        {
             DBG("Running as TowelHost - waiting for rename.");
             status = LoadStatus::NoPlugin;
        }
        else
        {
            DBG("Could not determine valid executable/plugin name.");
            status = LoadStatus::NoPlugin;
            lastError = "Could not determine plugin name from executable.";
        }
    }

    // Destructor: Clean up resources in reverse order
    ~MainComponent() override
    {
        // Remove listener *before* destroying the editor
        if (pluginEditor != nullptr)
        {
            pluginEditor->removeComponentListener(this);
            DBG("Removed ComponentListener from plugin editor.");
        }

        audioEngine.stop(); // Stop audio processing first
        pluginEditor = nullptr; // Destroy editor component
        audioEngine.setProcessor(nullptr); // Unlink processor from audio engine
        loadedPlugin = nullptr; // Destroy plugin instance
    }

    // Paint: Draw status messages only if no editor is active
    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

        // Only draw messages if there's no active plugin editor
        if (pluginEditor == nullptr)
        {
            g.setColour(juce::Colours::white);
            juce::String message;
            int maxLines = 2; // Default
            switch (status)
            {
                case LoadStatus::NoPlugin:
                    message = "Rename this app to match your plugin's name (e.g., 'MyPlugin.app' to load 'MyPlugin.component')";
                    maxLines = 3;
                    break;

                case LoadStatus::LoadFailed:
                    g.setColour(juce::Colours::red);
                    message = "Failed to load plugin: " + lastError;
                    break;

                case LoadStatus::LoadedNoEditor:
                    message = "Plugin loaded successfully but has no editor interface.";
                    break;

                case LoadStatus::LoadedWithEditor:
                    message = "Error: Status is LoadedWithEditor but editor is null.";
                     g.setColour(juce::Colours::orange);
                    break;
            }
            if (!message.isEmpty())
                g.drawFittedText(message, getLocalBounds().reduced(10), juce::Justification::centred, maxLines);
        }
    }

    // Resized: Called when *this* MainComponent's size changes.
    // This happens initially, when the user resizes the window,
    // OR when we call setSize() in response to a plugin resize.
    void resized() override
    {
        // 1. Make the editor fill the new bounds of this component
        if (pluginEditor != nullptr)
        {
            pluginEditor->setBounds(getLocalBounds());
            DBG("MainComponent::resized() - Set editor bounds to: " + getLocalBounds().toString());
        }

        // 2. Tell the parent window to adjust its content size to match ours
        updateParentWindowSize();
    }

    // --- ComponentListener Callback ---
    // Called when a component we are listening to (the pluginEditor) is moved or resized.
    void componentMovedOrResized(juce::Component& component, bool wasMoved, bool wasResized) override
    {
        // Check if the notification is from our plugin editor and if it was resized
        if (&component == pluginEditor.get() && wasResized)
        {
            int editorWidth = pluginEditor->getWidth();
            int editorHeight = pluginEditor->getHeight();
            DBG("ComponentListener: Plugin editor resized itself to " + juce::String(editorWidth) + "x" + juce::String(editorHeight));

            // Check if the editor's new size is different from our current size
            if (editorWidth > 0 && editorHeight > 0 && (editorWidth != getWidth() || editorHeight != getHeight()))
            {
                DBG("ComponentListener: Editor size differs from MainComponent size. Resizing MainComponent...");
                // Resize this MainComponent to match the editor's new size.
                // This will trigger MainComponent::resized() above.
                setSize(editorWidth, editorHeight);
            }
            else if (editorWidth <= 0 || editorHeight <= 0)
            {
                DBG("ComponentListener: Editor reported invalid size (" + juce::String(editorWidth) + "x" + juce::String(editorHeight) + "), ignoring.");
            }
            else
            {
                 DBG("ComponentListener: Editor size matches MainComponent size, no action needed.");
            }
        }
    }

private:
    // Helper function to update the parent window's size
    void updateParentWindowSize()
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        {
            int currentWidth = getWidth();
            int currentHeight = getHeight();

            // Check if the window's *content* size already matches
            // (prevents redundant updates and potential flicker)
            auto currentContentBounds = dw->getContentComponent() ? dw->getContentComponent()->getBounds() : juce::Rectangle<int>{};
            if (currentContentBounds.getWidth() != currentWidth || currentContentBounds.getHeight() != currentHeight)
            {
                DBG("Updating parent window content size to: " + juce::String(currentWidth) + "x" + juce::String(currentHeight));
                dw->setContentComponentSize(currentWidth, currentHeight);
            }
        }
    }

    // Load Plugin Logic
    void loadPluginFromName(const juce::String& name)
    {
        DBG("Attempting to load plugin: " + name);
        lastError = ""; // Clear previous error

        // Try loading plugin
        loadedPlugin = pluginLoader.loadPlugin(name);

        if (!loadedPlugin)
        {
            status = LoadStatus::LoadFailed;
            lastError = pluginLoader.getLastError().isNotEmpty() ? pluginLoader.getLastError() : "Could not find or load \"" + name + "\"";
            DBG(lastError);
            setSize(400, 200); // Ensure default size for error message
            return; // Exit loading process
        }

        // --- Plugin loaded successfully ---
        DBG("Plugin instance created: " + loadedPlugin->getName());
        loadedPlugin->enableAllBuses();
        audioEngine.setProcessor(loadedPlugin.get());

        if (loadedPlugin->hasEditor())
        {
            DBG("Plugin reports having an editor. Creating...");
            juce::AudioProcessorEditor* editor = loadedPlugin->createEditor();

            if (editor != nullptr)
            {
                DBG("Editor created successfully.");
                pluginEditor.reset(editor); // Take ownership

                addAndMakeVisible(pluginEditor.get());

                int width = pluginEditor->getWidth();
                int height = pluginEditor->getHeight();
                DBG("Editor initial size reported: " + juce::String(width) + "x" + juce::String(height));

                const int defaultWidth = 600;
                const int defaultHeight = 400;
                if (width <= 0) width = defaultWidth;
                if (height <= 0) height = defaultHeight;

                // Set *this* component's size FIRST. This triggers MainComponent::resized(),
                // which sets editor bounds AND calls updateParentWindowSize().
                setSize(width, height);
                DBG("MainComponent initial size set to: " + juce::String(width) + "x" + juce::String(height));

                // ** NOW add the listener for future changes **
                pluginEditor->addComponentListener(this);
                DBG("Added ComponentListener to plugin editor.");

                status = LoadStatus::LoadedWithEditor;
                audioEngine.start();

                // Set initial window resizability based on editor
                if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
                {
                    bool canResize = true;
                    if (pluginEditor->getConstrainer() != nullptr) {
                        canResize = (pluginEditor->getConstrainer()->getMinimumWidth() != pluginEditor->getConstrainer()->getMaximumWidth()) ||
                                    (pluginEditor->getConstrainer()->getMinimumHeight() != pluginEditor->getConstrainer()->getMaximumHeight());
                    } else if (!pluginEditor->isResizable()) {
                        canResize = false;
                    }
                    dw->setResizable(canResize, canResize);

                    if (auto* constrainer = pluginEditor->getConstrainer())
                        dw->setResizeLimits(constrainer->getMinimumWidth(), constrainer->getMinimumHeight(),
                                         constrainer->getMaximumWidth(), constrainer->getMaximumHeight());
                    else if (!canResize)
                        dw->setResizeLimits(width, height, width, height);
                }

                DBG("Plugin loading complete with editor.");
                return; // Success
            }
            else
            {
                DBG("Plugin reported hasEditor() but createEditor() returned nullptr.");
                // Fall through to LoadedNoEditor status
            }
        }
        else
        {
            DBG("Plugin does not have an editor interface.");
        }

        // --- Plugin loaded, but no editor ---
        status = LoadStatus::LoadedNoEditor;
        setSize(400, 200); // Ensure default size for message
        audioEngine.start(); // Start audio processing anyway
        DBG("Plugin loading complete without editor.");
        // No listener needed if there's no editor
    }

    PluginLoader pluginLoader;
    AudioEngine audioEngine;
    std::unique_ptr<juce::AudioPluginInstance> loadedPlugin;
    std::unique_ptr<juce::AudioProcessorEditor> pluginEditor;

    // State Members
    LoadStatus status { LoadStatus::NoPlugin };
    juce::String lastError;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

//==============================================================================
class TowelHostApplication : public juce::JUCEApplication
{
public:
    TowelHostApplication() {}

    const juce::String getApplicationName() override
    {
        return PluginLoader::getExecutableName();
    }

    const juce::String getApplicationVersion() override { return "1.0.3"; } // Version bump

    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    // --- MainWindow nested class ---
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                 .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            // **Crucially, setResizable true here so MainComponent can adjust if needed**
            // MainComponent might later refine this based on the plugin's capabilities.
            setResizable(true, true);

            // 1. Create the MainComponent
            auto mainComp = std::make_unique<MainComponent>();

            // 2. Get its final initial size
            int contentWidth = mainComp->getWidth();
            int contentHeight = mainComp->getHeight();
            DBG("MainWindow: MainComponent initial size = " + juce::String(contentWidth) + "x" + juce::String(contentHeight));

            // 3. Set content (window takes ownership)
            // Note: mainComp is now null after release()
            setContentOwned(mainComp.release(), true);

            // 4. Size the Window based on Content Size
            const int minWidth = 300;
            const int minHeight = 150;
            centreWithSize(juce::jmax(minWidth, contentWidth),
                           juce::jmax(minHeight, contentHeight));

            // 5. Make visible
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };
    // --- End MainWindow nested class ---

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION(TowelHostApplication)
