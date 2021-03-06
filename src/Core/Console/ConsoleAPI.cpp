/**
 *  For conditions of distribution and use, see copyright notice in license.txt
 *
 *  @file   ConsoleAPI.cpp
 *  @brief  Console core API.
 */

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "ConsoleAPI.h"
#include "ConsoleWidget.h"
#include "ShellInputThread.h"

#include "Profiler.h"
#include "Framework.h"
#include "InputAPI.h"
#include "UiAPI.h"
#include "UiGraphicsView.h"
#include "LoggingFunctions.h"
#include "FunctionInvoker.h"

#include "MemoryLeakCheck.h"

ConsoleAPI::ConsoleAPI(Framework *fw) :
    QObject(fw),
    framework(fw),
    enabledLogChannels(LogLevelErrorWarnInfo)
{
    if (!fw->IsHeadless())
        consoleWidget = new ConsoleWidget(framework);

    inputContext = framework->Input()->RegisterInputContext("Console", 100);
    inputContext->SetTakeKeyboardEventsOverQt(true);
    connect(inputContext.get(), SIGNAL(KeyEventReceived(KeyEvent *)), SLOT(HandleKeyEvent(KeyEvent *)));

    RegisterCommand("help", "Lists all registered commands.", this, SLOT(ListCommands()));
    RegisterCommand("clear", "Clears the console widget's log.", this, SLOT(ClearLog()));
    RegisterCommand("loglevel", "Sets the current log level. Call with one of the parameters \"error\", \"warning\", \"info\", or \"debug\".",
        this, SLOT(SetLogLevel(const QString &)));

    shellInputThread = boost::shared_ptr<ShellInputThread>(new ShellInputThread);

    QStringList logLevel = fw->CommandLineParameters("--loglevel");
    if (logLevel.size() == 1)
        SetLogLevel(logLevel[0]);
}

ConsoleAPI::~ConsoleAPI()
{
    Reset();
}

void ConsoleAPI::Reset()
{
    commands.clear();
    inputContext.reset();
    SAFE_DELETE(consoleWidget);
    shellInputThread.reset();
}

QVariant ConsoleCommand::Invoke(const QStringList &params)
{
    QVariant returnValue;

    // If we have a target QObject, invoke it.
    if (target)
    {
        FunctionInvoker fi;
        QString errorMessage;
        fi.Invoke(target, functionName, params, &returnValue, &errorMessage);
        if (!errorMessage.isEmpty())
            LogError("ConsoleCommand::Invoke returned an error: " + errorMessage);
    }

    // Also, there may exist a script-registered handler that implements this console command - invoke it.
    emit Invoked(params);

    return returnValue;
}

ConsoleCommand *ConsoleAPI::RegisterCommand(const QString &name, const QString &desc)
{
    if (commands.find(name) != commands.end())
    {
        LogWarning("ConsoleAPI: Command " + name + " is already registered.");
        return commands[name].get();
    }

    boost::shared_ptr<ConsoleCommand> command = boost::shared_ptr<ConsoleCommand>(new ConsoleCommand(name, desc, 0, ""));
    commands[name] = command;
    return command.get();
}

void ConsoleAPI::RegisterCommand(const QString &name, const QString &desc, QObject *receiver, const char *memberSlot)
{
    if (commands.find(name) != commands.end())
    {
        LogWarning("ConsoleAPI: Command " + name + " is already registered.");
        return;
    }

    boost::shared_ptr<ConsoleCommand> command = boost::shared_ptr<ConsoleCommand>(new ConsoleCommand(name, desc, receiver, memberSlot+1));
    commands[name] = command;
}

/// Splits a string of form "MyFunctionName(param1, param2, param3, ...)" into
/// a commandName = "MyFunctionName" and a list of parameters as a StringList.
void ParseCommand(QString command, QString &commandName, QStringList &parameterList)
{
    command = command.trimmed();
    if (command.isEmpty())
        return;

    int split = command.indexOf("(");
    if (split == -1)
    {
        commandName = command;
        return;
    }

    commandName = command.left(split).trimmed();
    parameterList = command.mid(split+1).split(",");
}

void ConsoleAPI::ExecuteCommand(const QString &command)
{
    PROFILE(ConsoleAPI_ExecuteCommand);

    QString commandName;
    QStringList parameterList;
    ParseCommand(command, commandName, parameterList);
    if (commandName.isEmpty())
        return;

    CommandMap::iterator iter = commands.find(commandName);
    if (iter == commands.end())
    {
        LogError("Cannot find a console command \"" + commandName + "\"!");
        return;
    }

    iter->second->Invoke(parameterList);
}

void ConsoleAPI::Print(const QString &message)
{
    if (consoleWidget)
        consoleWidget->PrintToConsole(message);
    printf("%s", message.toStdString().c_str());
    ///\todo Temporary hack which appends line ending in case it's not there (output of console commands in headless mode)
    if (!message.endsWith("\n"))
        printf("\n");
}

void ConsoleAPI::ListCommands()
{
    Print("Available console commands:");
    for(CommandMap::iterator iter = commands.begin(); iter != commands.end(); ++iter)
        Print(iter->first + " - " + iter->second->Description());
}

void ConsoleAPI::ClearLog()
{
    if (consoleWidget)
        consoleWidget->ClearLog();
}

void ConsoleAPI::SetLogLevel(const QString &level)
{
    if (level.compare("error", Qt::CaseInsensitive) == 0)
        SetEnabledLogChannels(LogLevelErrorsOnly);
    else if (level.compare("warning", Qt::CaseInsensitive) == 0)
        SetEnabledLogChannels(LogLevelErrorWarning);
    else if (level.compare("info", Qt::CaseInsensitive) == 0)
        SetEnabledLogChannels(LogLevelErrorWarnInfo);
    else if (level.compare("debug", Qt::CaseInsensitive) == 0)
        SetEnabledLogChannels(LogLevelErrorWarnInfoDebug);
    else
        LogError("Unknown parameter \"" + level + "\" specified to ConsoleAPI::SetLogLevel!");
}

void ConsoleAPI::Update(f64 frametime)
{
    PROFILE(ConsoleAPI_Update);

    std::string input = shellInputThread->GetLine();
    if (input.length() > 0)
        ExecuteCommand(input.c_str());
}

void ConsoleAPI::ToggleConsole()
{
    if (consoleWidget)
        consoleWidget->ToggleConsole();
}

void ConsoleAPI::HandleKeyEvent(KeyEvent *e)
{
    const QKeySequence &toggleConsole = framework->Input()->KeyBinding("ToggleConsole", QKeySequence(Qt::Key_F1));
    if (e->sequence == toggleConsole)
        ToggleConsole();
}

void ConsoleAPI::LogInfo(const QString &message)
{
    ::LogInfo(message);
}

void ConsoleAPI::LogWarning(const QString &message)
{
    ::LogWarning(message);
}

void ConsoleAPI::LogError(const QString &message)
{
    ::LogError(message);
}

void ConsoleAPI::LogDebug(const QString &message)
{
    ::LogDebug(message);
}

void ConsoleAPI::Log(u32 logChannel, const QString &message)
{
    if (!IsLogChannelEnabled(logChannel))
        return;

    Print(message);
}

void ConsoleAPI::SetEnabledLogChannels(u32 newChannels)
{
    enabledLogChannels = newChannels;
}

bool ConsoleAPI::IsLogChannelEnabled(u32 logChannel) const
{
    return (enabledLogChannels & logChannel) != 0;
}

u32 ConsoleAPI::EnabledLogChannels() const
{
    return enabledLogChannels;
}

