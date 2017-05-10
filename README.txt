=================================================================================================
 Description
=================================================================================================
	+ A simple TCP file transfer server and client.
	
	+ The client is able to connect to the server and download/upload files, as well as
	  execute simple commands on the server.
	
	+ The client also acts as a simple shell, its able to fork and exec commands, and
	  change directories.
	
	+ The server can support multiple clients at the same time.
=================================================================================================




=================================================================================================
 How To Use:
=================================================================================================
	1. Build the server and client.
	       Build Server: run "make -f Makefile-server"
	       Build Client: run "make -f Makefile-client"
	
	2. Start the server on a machine and give it a port (preferably a non well known port).
	       Example: ./server 12345
	
	3. Connect to the server using the client.
	       Examples:
	          If the server is started on the local computer:
	            ./client 127.0.0.1 12345
	       
	          If the server is started on another computer:
                ./client [the servers ip] 12345
=================================================================================================




=================================================================================================
 Commands
=================================================================================================
    To get some help type 'help' in the client.
    
	+ Commands which are sent to the server from the client (These commands look like standard
	  Unix commands but are prefixed with an s, this allows the server to simply get rid of the
	  leading 's' in these commands and use popen() to open a pipe to the command, an added
	  bonus of this is that whatever arguments these commands usually accept, are also available
	  here):
		sls     - Server list files.
		spwd    - Server print the current working directory.
		scd     - Server change directory.
		smd5sum - Server compute the md5 hash of a file.
	
	+ Commands which execute on the client:
		q     - Close the connection and exit.
		help  - Print out the help screen.
		Other - Commands which can be executed in the regular shell (ls, cd, pwd, mkdir, rm, vim,
		        etc.) can be executed in the client.
	
	+ Download and upload commands:
		get - Download a file from the server into the clients current working directory.
		put - Upload a file to the servers current working directory.
=================================================================================================




=================================================================================================
 Notes
=================================================================================================
	Verify the integrity of a downloaded or uploaded file:
		1. Use the command "smd5sum" to compute the md5 hash of the file you want to download,
		   record this value for later.
		2. Download the file.
		3. Use the command "md5sum" to compute the md5 hash of the downloaded file.
		4. If the two values dont match, then some error occured during transmission, it is up
		   to the user to determine whether they want to try downloading/uploading the file
		   again or not.
=================================================================================================




=================================================================================================
 Protocols
=================================================================================================
	legend:
		[] - Square brackets indicate an array of byte literals.
		     For example:
		       [OK1234] would be equivilant to: 
		         array[0] = 'O';
		         array[1] = 'K';
		         array[2] = 1;
		         and so on.
	
	What happends when an error occurs:
		I could not get a reliable error handling system in place for this client/server.
		Because of this, the server/client hang up and need to be restarted occasionally on bad
		connections.
		
		The best I could do in the time I had was to make the executeCommand() functions in both
		the client and server return -1,0 or 1.
		
		 0: Everything went OK.
		
		 1: Non-critical error.
		      For example: A file could not be opened, or a command was not in the correct format.
		                   Both the client and server can continue running.
		
		-1: Critical error, server/client just terminate the connection and exit.
		      For example: server/client closed connection, or one of them crashed.
	
	scd, spwd, sls, smd5sum:
		1. Client sends null terminated string in the format: "sxxxx [Arguments]".
		     Examples:
		       "scd /home"
		       "sls"
		       "sls /bin"
		       "spwd"
		       "smd5sum file1"
		
		2. Server recieves command, skips the leading 's', uses popen() to open a pipe to the
		   output of the command and sends the output of the command until EOF.
		
		3. Once EOF is read from the pipe, a null terminated buffer is sent to tell the client
		   to stop reading.
		
		Example:
		  Client sends: "sls /home\0"
		  Server interprets command as "ls /home\0", and opens a pipe to it
		  Lets say the output was:
		    --Start ls output
		    mark
		    guest1
		    david
		    guest2
		    --END ls output
		  Then the packet sent would be "mark\nguest1\ndavid\nguest2\n\0"
		  The client stops reading once it reads the '\0' character.
	
	get:
		1.  Client sends a null terminated string in the following format: "get FILEPATH\0".
		
		2.  Client waits for servers reply.
		
		3a. If the server replies with a message of [OKxxxxxxxx], then the file was succesfully
		    opened. The value xxxxxxxx represents a long value which indicates the size of the
		    file to be downloaded.
		
		3b. If the server replies with a message of [NOxxxxxxxx], then the file could not be
		    opened, or it was a directory. In this case, xxxxxxxx is simply padding and should
		    be ignored. A packet containing the error message follows afterwards.
	
	put:
		1.  Client sends a null terminated string in the format: "put FILENAME". Note that it is 
		    a FILE NAME which is sent, not a file path, this is because a client is only able to
		    upload in the current working directory of the server.
		
		2.  Client waits for reply.
		
		3a. If the server replies with [NO], then the client can not upload the file. The server
		    sends an error message indicating why the file can not be uploaded, and goes back to
		    listening for the next command.
		
		3b. If the server replies with [OK], then the file is ok to upload, proceed with the
		    next steps.
		
		4.  Client sends the size of the file.
		
		5.  Client sends the contents of the file.
=================================================================================================





