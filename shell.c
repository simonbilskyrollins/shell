// Simon Bilsky-Rollins
// CS 332 Shell Project

#include    <stdlib.h>
#include    <stdio.h>
#include    <fcntl.h>
#include    <unistd.h>
#include    <string.h>
#include    <sys/types.h>
#include    <sys/wait.h>


typedef enum { false, true } bool;

char** readLineOfWords(void);

int
main()
{
  for (;;)
    {
      // Display prompt when ready for new user input
      printf("\x1B[36msimpleShell\x1B[0m $ ");

      char** words = readLineOfWords();

      // Exit on EOF input
      if (words == NULL)
        {
          printf("\n");
          return 0;
        }

      bool forked = false;
      bool redirect_in = false;
      bool redirect_out = false;
      bool piped = false;
      bool abort = false;

      char* infile;
      char* outfile;

      // Create pipes array to hold pointers to beginning of each piped command
      size_t MAX_NUM_PIPES = 26;
      char** pipes[MAX_NUM_PIPES];
      pipes[0] = &words[0];

      // prints the tokens in the array separated by spaces
      int i=0;
      int current_pipe = 1;
      while (words[i] != NULL)
        {
          // Series of string comparisons to detect all special operators in the command
          int forked_cmp = strncmp(words[i], "&", 1);
          if (forked_cmp == 0)
            {
              forked = true;
              words[i] = NULL;
              break;
            }
          int redirect_in_cmp = strncmp(words[i], "<", 1);
          if (redirect_in_cmp == 0)
            {
              redirect_in = true;
              words[i] = NULL;
              infile = words[i+1];
              if (infile == NULL)
                {
                  printf("ERROR: no input file specified after <\n");
                  abort = true;
                  break;
                }
              i++;
            }
          int redirect_out_cmp = strncmp(words[i], ">", 1);
          if (redirect_out_cmp == 0)
            {
              redirect_out = true;
              words[i] = NULL;
              outfile = words[i+1];
              if (outfile == NULL)
                {
                  printf("ERROR: no output file specified after >\n");
                  abort = true;
                  break;
                }
              i++;
            }
          int pipe_cmp = strncmp(words[i], "|", 1);
          if (pipe_cmp == 0)
            {
              piped = true;
              words[i] = NULL;
              char* next_word = words[i+1];
              if (next_word == NULL)
                {
                  printf("ERROR: null input after pipe operator\n");
                  abort = true;
                  break;
                }
              pipes[current_pipe] = &words[i+1];
              current_pipe++;
              pipes[current_pipe] = NULL;
            }
          i++;
        }

      // Reset to beginning of for loop if an error occurred
      if (abort) continue;

      if (piped)
        {
          // Pipe input starts as stdin, or whatever file has been redirected to stdin
          int pipe_in = 0;
          if (redirect_in)
            {
              pipe_in = open(infile, O_RDONLY);
            }

          // Loop through all piped commands
          for (i=0; i < current_pipe; i++)
            {
              char** pipe_words = pipes[i];

              int pipe_fd[2];
              pipe(pipe_fd);

              int pid = fork();
              if (pid == 0)
                {
                  int pipe_out = pipe_fd[1];

                  // Replace stdin with piped input
                  dup2(pipe_in, 0);
                  close(pipe_in);

                  if (i < current_pipe - 1)
                    {
                      // Replace stdout with piped output
                      dup2(pipe_out, 1);
                      close(pipe_out);
                    }
                  else
                    {
                      // On the last piped command, output goes to stdout or a redirect
                      if (redirect_out)
                        {
                          int out_fd = open(outfile, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                          dup2(out_fd, 1);
                          close(out_fd);
                        }
                    }

                  int err = execvp(pipe_words[0], pipe_words);
                  printf("execvp() returned with error code %d", err);
                }
              else
                {
                  // All we need to do in the parent process is wait for the
                  // last piped command to exit (or not, if the user forked)
                  close(pipe_fd[1]);
                  if (i == current_pipe - 1)
                    {
                      if (forked)
                        {
                          printf("Forked process %s to pid %d\n", words[0], pid);
                        }
                      else
                        {
                          waitpid(pid, NULL, 0);
                        }
                    }
                }
              // Set the input for the next piped command
              pipe_in = pipe_fd[0];
            }
          continue;
        }

      int pid = fork();
      if (pid == 0)
        {
          // Child process. Check for redirects and then execute command
          if (redirect_in)
            {
              int in_fd = open(infile, O_RDONLY);
              dup2(in_fd, 0);
              close(in_fd);
            }
          if (redirect_out)
            {
              int out_fd = open(outfile, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
              dup2(out_fd, 1);
              close(out_fd);
            }
          int err = execvp(words[0], words);
          printf("execvp() returned with error code %d", err);
        }
      else
        {
          // Parent process. Wait for child to exit unless it was forked by the user
          if (forked)
            {
              printf("Forked process %s to pid %d\n", words[0], pid);
            }
          else
            {
              waitpid(pid, NULL, 0);
            }
        }
    }
}

/*
 * Took this function from execexample.c by Sherri Goings:
 * reads a single line from terminal and parses it into an array of tokens/words
 * by splitting the line on spaces.  Adds NULL as final token
 */
char**
readLineOfWords()
{
  // A line may be at most 100 characters long, which means longest word is 100
  // chars, and max possible tokens is 51 as must be space between each
  size_t MAX_WORD_LENGTH = 100;
  size_t MAX_NUM_WORDS = 51;

  // allocate memory for array of array of characters (list of words)
  char** words = (char**) malloc( MAX_NUM_WORDS * sizeof(char*) );
  int i;
  for (i=0; i<MAX_NUM_WORDS; i++) {
    words[i] = (char*) malloc( MAX_WORD_LENGTH );
  }

  // read actual line of input from terminal
  int bytes_read;
  char *buf;
  buf = (char*) malloc( MAX_WORD_LENGTH+1 );
  bytes_read = getline(&buf, &MAX_WORD_LENGTH, stdin);

  // check if EOF (or other error)
  if (bytes_read == -1) return NULL;

  // take each word from line and add it to next spot in list of words
  i=0;
  char* word = (char*) malloc( MAX_WORD_LENGTH );
  word = strtok(buf, " \n");
  while (word != NULL && i<MAX_NUM_WORDS) {
    strcpy(words[i++], word);
    word = strtok(NULL, " \n");
  }

  // check if we quit because of going over allowed word limit
  if (i == MAX_NUM_WORDS) {
    printf( "WARNING: line contains more than %d words!\n", (int)MAX_NUM_WORDS );
  }
  else words[i] = NULL;

  // return the list of words
  return words;
}
