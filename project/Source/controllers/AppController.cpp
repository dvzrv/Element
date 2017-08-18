
#include "controllers/AppController.h"
#include "controllers/EngineController.h"
#include "controllers/GuiController.h"
#include "engine/GraphProcessor.h"
#include "gui/GuiApp.h"
#include "gui/UnlockForm.h"
#include "session/UnlockStatus.h"
#include "Globals.h"
#include "Messages.h"
#include "Settings.h"

namespace Element {

AppController::AppController (Globals& g)
    : world (g)
{
    gui = GuiApp::create (g, *this);
    addChild (new GuiController());
    addChild (new EngineController());
    g.getCommandManager().registerAllCommandsForTarget (this);
    g.getCommandManager().setFirstCommandTarget (this);
}

AppController::~AppController() { }

void AppController::run()
{
    activate();
    gui->run();
}

void AppController::handleMessage (const Message& msg)
{
    bool handled = true;

    if (const auto* m = dynamic_cast<const LoadPluginMessage*> (&msg))
    {
        if (auto* ec = findChild<EngineController>())
            ec->addPlugin (m->description);
    }
    else if (const auto* m = dynamic_cast<const RemoveNodeMessage*> (&msg))
    {
        if (auto* ec = findChild<EngineController>())
            ec->removeNode (m->nodeId);
    }
    else if (const auto* m = dynamic_cast<const AddConnectionMessage*> (&msg))
    {
        if (auto* ec = findChild<EngineController>())
        {
            if (m->useChannels())
                ec->connectChannels (m->sourceNode, m->sourceChannel, m->destNode, m->destChannel);
            else
                ec->addConnection (m->sourceNode, m->sourcePort, m->destNode, m->destPort);
        }
    }
    else if (const auto* m = dynamic_cast<const RemoveConnectionMessage*> (&msg))
    {
        if (auto* ec = findChild<EngineController>())
        {
            if (m->useChannels()) {
                jassertfalse; // channels not yet supported
            }
            else
            {
                ec->removeConnection (m->sourceNode, m->sourcePort, m->destNode, m->destPort);
            }
        }
    }
    else
    {
        handled = false;
    }
    
    if (! handled)
    {
        DBG("[EL] AppController: unhandled Message received");
    }
}


ApplicationCommandTarget* AppController::getNextCommandTarget()
{
    return gui.get();
}

void AppController::getAllCommands (Array<CommandID>& commands)
{
    commands.addArray ({
        Commands::mediaNew,  Commands::mediaOpen,
        Commands::mediaSave, Commands::mediaSaveAs,
        Commands::signIn,    Commands::signOut
    });
}

void AppController::getCommandInfo (CommandID command, ApplicationCommandInfo& result)
{
    if (gui)
        gui->getCommandInfo (command, result);
}

bool AppController::perform (const InvocationInfo& info)
{
    EngineController* ec = findChild<EngineController>();
    jassert(ec);
    
    auto& status (world.getUnlockStatus());
    bool res = true;
    switch (info.commandID)
    {
        case Commands::mediaNew: {
            if (AlertWindow::showOkCancelBox (AlertWindow::InfoIcon, "New Graph", "This will clear the current graph, are you sure?"))
            {
                lastSavedFile = File();
                ec->clear();
            }
        } break;
            
        case Commands::mediaOpen:
        {
            FileChooser chooser ("Open a graph", File(), "*.elgraph;*.elg");
            if (chooser.browseForFileToOpen())
            {
                lastSavedFile = chooser.getResult();
                FileInputStream stream (lastSavedFile);
                const ValueTree g (ValueTree::readFromStream (stream));
                if (g.isValid())
                {
                    const Node node (g, false);
                    ec->setRootNode (node);
                }
            }
        } break;
            
        case Commands::mediaSave:
        {
            if (! status.isUnlocked())
            {
                AlertWindow::showMessageBox (AlertWindow::InfoIcon,
                    "Unauthorized", "Saving is only available with a paid version of this software. Visit https://kushview.net/products/element to purchase a copy");
            }
            else
            {
                GraphProcessor& graph (world.getAudioEngine()->graph());
                
                if (lastSavedFile.existsAsFile() && lastSavedFile.hasFileExtension ("elgraph"))
                {
                    const ValueTree model (graph.getGraphState());
                    FileOutputStream stream (lastSavedFile);
                    model.writeToStream (stream);
                }
                
                else
                {
                    FileChooser chooser ("Save current graph", File(), "*.elgraph");
                    if (chooser.browseForFileToSave (true))
                    {
                        lastSavedFile = chooser.getResult();
                        const ValueTree model (graph.getGraphState());
                        FileOutputStream stream (lastSavedFile);
                        model.writeToStream (stream);
                    }
                }
            }
        } break;
        
        case Commands::mediaSaveAs: {
            if (! status.isUnlocked())
            {
                AlertWindow::showMessageBox (AlertWindow::InfoIcon,
                    "Unauthorized", "Saving is only available with a paid version of this software. Visit https://kushview.net/products/element to purchase a copy");
            }
            else
            {
                GraphProcessor& graph (world.getAudioEngine()->graph());
                FileChooser chooser ("Save current graph", File(), "*.elgraph");
                if (chooser.browseForFileToSave (true))
                {
                    lastSavedFile = chooser.getResult();
                    const ValueTree model (graph.getGraphState());
                    FileOutputStream stream (lastSavedFile);
                    model.writeToStream (stream);
                }
            }
        } break;
        
        case Commands::signIn:
        {
            auto& status (getGlobals().getUnlockStatus());
            auto& settings (getGlobals().getSettings());
            {
                auto* form = new UnlockForm (status, "Enter your license key.",
                                             false, false, true, true);
                DialogWindow::LaunchOptions opts;
                opts.content.setOwned (form);
                opts.resizable = false;
                opts.dialogTitle = "Authorization";
                opts.runModal();
            }
        } break;
        
        case Commands::signOut:
        {
            auto& status (getGlobals().getUnlockStatus());
            auto& settings (getGlobals().getSettings());
            if (status.isUnlocked())
            {
                auto* props = settings.getUserSettings();
                props->removeValue("L");
                props->save();
                status.load();
            }
        } break;
        
        default: res = false; break;
    }
    return res;
}

}
