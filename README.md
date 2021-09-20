DESIGN OVERVIEW:
    When run, the program will setup multiple necessary global variables and, in
    main, determine from the input whether BatchMode or InteractiveMode should
    be called. If a batchfile is given on running the program, main passes it to
    BatchMode, which reads through it and executes any commands found within. If
    no batchfile is given, InteractiveMode is called and begins prompting the
    user for commands.
    
    InteractiveMode will check user input to make sure it's valid before passing
    it to ParseCommands, which parses input line into separate commands, erasing
    any unnecessary whitespace and ignoring empty commands, and passes each 
    command into ExecuteCommands. The command will be further parsed into its
    arguments via ParseArgs, which will then be passed to various specific
    functions for commands like "cd", "exit", "path", "myhistory", and "alias",
    or into CommandRedirect for more general commands.
    
    CommandRedirect will first check if there is any pipelining necessary, and
    if so will pass the command into PipeCommands. If there is none, it will
    execute the commands with any necessary redirection and return back to the
    user prompt. PipeCommands will execute the commands via pipes and similarly
    return back to the user prompt.

How to COMPILE:
    make

HOW TO RUN:
    ./output
    OR
    ./output [batchfile]

HOW TO CLEAN:
    make clean
