==============================================================
 NOTE: This repo is already way ahead of FIRST-BIG-THING
       The next steps are already being implemented too,
	   so we can see in which direction we should be going...
==============================================================


The current problem:

When readinng the settings file a lot of settings are immediately activated, while the application is not yet started and dialogs are not yet created/shown.
This causes a lot of, unneccesary initialisations and errorhandling is sometimes completely missing.
Also it takes a very long time before any window shows up after starting Jamulus.


The general idea of how it should be:
	CSettings should NOT do any more than just reading the inifile at startup (constructor) and save the current settings at shutdown (desstructor).
	All other classes will use the values in Settings directly whenever possible. (So no local values are used when not strictly neccesary.)
	When the use of local variables is inevitable those classes should implement the OnAboutToQuit() slot and update Settings with the local values before shutdown.
	Non-inifile settings/status values that are used in (settings) dialogs should also be located in Settings or a separate Status class.
	Also the commandline parameters should be available via Settings.


At startup we now can:

1 First read the essential commandline parameters --help, --version, --server and --nogui.
2 Read the remaining commandline parameters via a CCommandlineparameters class in settings
2 Create the Client/Server instance which will include Settings which reads commandline parameters and the inifile.
3 Create and show any dialogs
4 Let Client/Server Activate all settings (a call to ApplySettings() ?)
5 Start the Q(Core)Application...


Notes of attention:

How to handle overriding commandline parameters?
    Do we actually want to save the commandline values to the inifile or preserve the original values?
    I suggest: (here already being implemented, WIP)
        The global CCommandlineOption class would be needed... 
        Settings should have GetXxx/SetXxx functions (actual values should be protected.)
        GetXxx returns Xxx Commandline value if given, otherwise it returns the inifile value
        SetXxx sets inifile value and resets any Xxx commandline parameter. 
        SetXxx should also emit a XxxChanged signal on change of a value.


We should add a --store commandline parameter to store all given commandline options in the inifile, 
    so on the next run we only have to give the inifile parameter (and -s/--server if starting in server mode).

Further future improvements: (Most already done/WIP)
	main should first check for --help and --version parameters.
	main should then check for the --server and --nogui parameters.
	all others should be read by a CCommandlineOptions class in CSettings.

	Move ClientSettings/ServerSettings to CClient/CServer (here already implemented)
	rpc server should also be instantiated in CClient/CServer ! (here already implemented)
	Also add a Status class to Client/Server with all, non-setting, current values. (WIP)


	Better solution for notifiers:
		Get/Set functions in Settings and Status classes and the Set functions should emit a "XxxChanged" notifier.
		GUI and RPC should just use Settings and Status classes and subscribe to their notifiers.

	After the sound-redesign implementation also CSound can directly use CSettings and slots
	This would also strongly reduce the number of needed CSoundbase functions and would improve efficiency.

	Store Sound settings per device: (since i.e. channel selection and input boost is device dependant)
		CSound should change all device settigs at once when selecting another device (DeviceChange notifier from CClientSettings ?).

	Final stage: All controls (GUI, RPC, MIDI) should only use notifiers !
	             No direct access to CCLient/CServer !


Order of implementations:
	Implement CMessages
	Implement CCommandline
	Implement fix for #2519 (Connection status client and gui can get out of sync)
	Implement #2575 (Move Sound sources)
	Implement sound-redesign
	Implement CCommandlineOptions and add CCommandlineOptions to CSettings (also a major change of main!)
	Move CClientSettings/CServerSettings to CClient/CServer
	Move rpc servers to CClient/CServer
	Add CClientStatus/CServerStatus to CClient/CServer
	Change all local Settings/Status variables in other classes to variables in Settings/Status (multiple PR's)
	Change direct access to Settings/Status variables to notifiers (multiple PR's)
	Change Settings to store soundcard settings per device. (might be implemented earlier though)
	Add --store commandline option to store the commandline (except --inifile and --store itself).
		if --store is given Jamulus will just store the commandline as default setting for the next startup and exit.
		the stored commandline will be used if Jamulus is started with no parameters or just a --inifile.

More todo:
	cleanup/split util.h: (can be implemented at any time I guess)
		util.h is becomming a mixed bag...
		split into:
			CommonTypes.h
			CVector.h
			GuiTypes.h
			ProtocolTypes.h



