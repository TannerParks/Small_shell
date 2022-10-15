Small shell program written in C. Supports the following:

  - Background and foreground processes
      - Adding & at the end of a command executes it in the background
  - Variable expansion
      - $$ is replaced with pid
  - Signal handling
  - cd, status, and exit commands are implemented from scratch
  - All other commands are implemented using execvp
  - Input and output redirections
 
