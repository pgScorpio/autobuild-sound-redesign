The problem:

When readinng the settings file a lot of settings are immediately activated, while the dialogs are not yet created/shown.

This causes a lot of, unneccesary initialisations and errorhandling is sometimes missing.

Also it takes a very long time before any window shows up after starting Jamulus.



The general idea:

CSettings should NOT do any more than just reading the inifile at startup (constructor) and save the current settings at shutdown (desstructor).


All other classes will use the values in Settings directly whenever possible. (So no local values used when not strictly neccesary.)
When the use of local variables is inevitable those classes should implement the OnAboutToQuit() slot and update Settings before shutdown where needed.
Also non-inifile settings that are used in setting dialogs should also be located in Settings.

Dialogs/RPC will use/change values in Settings directly and emit a "Changed" notifier.


At startup we now can:

1 Create the Settings instance.
2 Initialise the translator
3 Create Client/Server
4 Create and show any dialogs
5 Let Client/Server Activate all settings (call to ApplySettings())
6 Start running...


Notes of attention:

How to handle overriding commandline parameters?
    The global commandline class would be needed...
    Do we actually want to save the commandline values to the inifile or preserve the original values?


Future improvements:

main should only read the --server commandline parameter
all others should be read by CCLientSettings/CServerSettings.

Better solution for notifiers: 
    Get/Set functions in Settings class and the Set functions should emit the "Changed" notifier from Settings.
    In this way we don't need separate notifiers from Dialogs and RPC.

After the sound-redesign implementation also CSound can directly use CSettings and slots
This would also strongly reduce the number of needed CSoundbase functions and would improve efficiency.
Also CClientSettings should no longer need calls to Client for Sound values, since they would be in Settings too.
And when we change stored Settings to store all Sound settings (like channel selection and input boost) per device
CSound can also change all settigs at once when selecting another device.

