The problem:

When readinng the settings file a lot of settings are immediately activated, while the dialogs are not yet created/shown.

This causes a lot of, unneccesary initialisations and errorhandling is sometimes completely missing.

Also it takes a very long time before any window shows up after starting Jamulus.



The general idea of how it should be:

CSettings should NOT do any more than just reading the inifile at startup (constructor) and save the current settings at shutdown (desstructor).


All other classes will use the values in Settings directly whenever possible. (So no local values are used when not strictly neccesary.)
When the use of local variables is inevitable those classes should implement the OnAboutToQuit() slot and update Settings with the local values before shutdown.
Non-inifile settings/status values that are used in (settings) dialogs should also be located in Settings.
Also the commandline parameters will be available via CSettings.

Dialogs/RPC will use/change values in Settings directly and Settings will emit "Changed" notifiers.


At startup we now can:

1 First read the essential commandline parameters --help, --version, --server and --nogui.
2 Read the commandline parameters via a CCommandlineparameters class that will be passed to settings.
2 Create the Client/Server Settings instance which will read the inifile.
4 Create Client/Server
5 Create and show any dialogs
6 Let Client/Server Activate all settings (a call to ApplySettings() ?)
7 Start the Q(Core)Application...


Notes of attention:

How to handle overriding commandline parameters?
    Do we actually want to save the commandline values to the inifile or preserve the original values?
    I suggest:
        The global CCommandlineOption class would be needed...
        Settings should have GetXxx/SetXxx functions (actual values should be protected.)
        GetXxx returns Xxx Commandline value if given, otherwise it returns the inifile value
        SetXxx sets inifile value and resets any Xxx commandline parameter. 
        SetXxx should also emit a XxxChanged signal on change of a value.


Future improvements:
    add a --store commandline parameter to store all given commandline options in the inifile, 
    so on the next run we only have to give the inifile parameter (and -s if starting in server mode).

main should first check for --help and --version parameters.
main should then check for the --server and --nogui parameters.
all others should be read by a CCommandlineOptions class that will be passed to Settings.

Better solution for notifiers:
    Get/Set functions in Settings class and the Set functions should emit the "Changed" notifier from Settings.
    In this way we don't need separate notifiers from Dialogs and RPC.

After the sound-redesign implementation also CSound can directly use CSettings and slots
This would also strongly reduce the number of needed CSoundbase functions and would improve efficiency.

Also CClientSettings should no longer need calls to Client for Sound values, since they would be in Settings too.
And when we change stored Sound settings to store them per device (like channel selection and input boost)
CSound can also change all settigs at once when selecting another device.
