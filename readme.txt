A pair of programs to support automatic backups from the client to the server.
C:\Main\Active\Documents\Programming\Rain\Templates\Rain Corporation Library added to include directories.

Server requests updates from clients.
Send Threads are implemented as Windows, for their messaging/queueing capabilities.

UPDATE	request client-side update of file status

1.1.0:
	better percentages
	faster server-side
	cleaner code
1.2.0:
	faster server during file transfer
	changed transfer format slightly
1.3.0:
	moved delim setting to .ini file
	added deletes in ServerStart freeing memory at end
	added icons to both
1.4.0:
	made server-side console cout more efficient during file transfer (significant speedup)
	switched out int for long long in some length-related processes
1.4.1:
	fixed IP address issues on server/storage directories
	notes: still issues with int/LL and large memory
1.4.2:
	fixed conversion and static_cast issues
	standardized to long long
1.5.0:
	server now tracks and updates file deletion
	added emp->fullmess.shrink_to_fit () to server, so that memory usage is dynamic according to what file it is processing
	fullmess size is now decided when reading file header and reserved
	notes: still doesn't update folder deletion, though folders will be empty
	notes: still issues with deletion checking
1.6.0:
	updated file deletion checking
	folder tracking implemented
	file buffering on client and server which limits memory usage while transferring files
	client.ini files doesn't need \ at the end of directories
	removed fullmess resizing
	tuned client side console messages
	fixed issue with empty files and percentages displayed
1.6.1:
	fixed issue with directories requiring multiple updates to delete
	fixed issue with empty directories not syncing
	notes: issues with Unicode filenames/directories
1.6.2:
	works with Unicode files/dirs
	notes: display is still messed up for Unicode, but that doesn't really matter
1.6.3:
	client now retries connection if server disconnects instead of closing
	fixed problem with updating filetimes with unicode files
	notes: unicode in console is impossible, unless the system supports it - for now we output multibyte junk, which will show unicode if the system is configured as such
1.6.4:
	server doesn't request updates if it is still receiving files
	failing while sending files now works for client - it returns to trying to connect to server