/* ********************* *
 * TP2 INF3173 H2021
 * Auteur: Hussein Nahle
 * ********************* */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// wait()
#include <sys/wait.h>
#include <sys/types.h>

// poll()
#include <poll.h>

// pipe()
#include <unistd.h>

#define USG_MSG "Usage: pat [-s separator] command arguments [+ command arguments]...\n"
#define ERR_CODE 127
#define DEFAULT_SEP 43

/* ********************************************************
 * pat_t aide à regrouper tous les informations en rapport
 * avec la commande entrée par l'utilisateur.
 * 
 *    flag_s      --> 1  l'option -s existe      
 *                --> 0  l'option -s n'existe pas
 * 
 *    sep         --> contient le séparateur. Par défaut 
 *                    c'est un "+"
 * 
 *    cmd_n       --> nombre de commande
 * 
 *    cmd_arr     --> tableau contenant les commandes
 * _________ ----------------------------------------------
 *  exemple | ./pat -s @@ cat prog1 @@ ./prog2
 * ---------'  
 * 
 *    flag_s = 1
 *    sep = "@@"
 *    cmd_n = 2
 *    cmd_arr = [[cat, prog1, NULL], [./prog2, NULL]]
 */

typedef struct
{
  int flag_s;
  char *sep;
  int cmd_n;
  char ***cmd_arr;
} pat_t;


/* ********************************************************
 * output_t aide à enregistrer les informations d'un message 
 * reçu provenant d'un certain tube et d'une certaine 
 * commande. (utiliser uniquement lors du poll)
 * 
 *    writenTo        --> 1  message provenant de stdout    
 *                    --> 2  message provenant de stderr
 * 
 *    endsWithnewLine --> 1 si le dernier caractère du message 
 *                          est égale à '\n'
 *                    --> 0 sinon
 * 
 *    cameFrom        --> numéro de la commande qui a écrit ce
 *                        message
 */

typedef struct
{
  int writenTo;   
  int endsWithNewLine;
  int cameFrom;
} output_t;


/* ********************************************************
 * Ressources globales. Utiles pour communiqué entre les
 * processus (utilisés seulement dans run (poll) et handler)
 */

output_t previous = {0, 1, 0};
output_t actual = {0, 1, 0};
char *separator = NULL;
int numberOfCmds = 1;
int status = 0;


// Condition 1 pour gérer l'affichage du séparateur
int canPrintSepWithNewLine()
{
  if (!previous.endsWithNewLine && ((actual.cameFrom != previous.cameFrom) ||
    previous.writenTo != actual.writenTo))
  {
    return 1;
  }  
  return 0;
}

// Condition 2 pour gérer l'affichage du séparateur
int canPrintSepWithoutNewLine()
{
  if ((previous.endsWithNewLine && (actual.cameFrom != previous.cameFrom)) ||
    ((actual.cameFrom == previous.cameFrom) && (previous.writenTo != actual.writenTo)))
  {
    return 1;
  }
  return 0;
}

/* ********************************************************
 * Méthode qui gère l'affichage des séparateurs
 *
 * Paramètres:
 *    msg     --> stdout, stderr, exit
 *    index   --> numéro de la commande
 */ 

void printSep(char* msg, int index)
{
  for (int i = 0; i < 3; i++)
  {
    printf("%s", separator);
  }

  if (numberOfCmds > 1)
  {
    printf(" %s %d", msg, index);
  }

  else if (numberOfCmds == 1)
  {
    printf(" %s", msg);
  }

  if (strncmp(msg, "exit", strsize(msg)))
  {
    printf("\n");
  }  
}

// Capturer les signals et gérer l'affichage du status/signal 
void handler(int sig)
{
  actual.writenTo = -1;
  actual.endsWithNewLine = 1;
  actual.cameFrom = previous.cameFrom;

  if (!previous.endsWithNewLine)
  {
    printf("\n%s", separator);
  }    
  printSep("exit", actual.cameFrom);

  int currentChildStatus;
  if (wait(&currentChildStatus) == -1)
  {
    fprintf(stderr, "pat: wait failed\n");
    free(separator);
    exit(ERR_CODE);
  }

  if (WIFEXITED(currentChildStatus))
  {
    currentChildStatus = WEXITSTATUS(currentChildStatus);
    printf(", status=%d\n", currentChildStatus);  
  }
  else if (WIFSIGNALED(currentChildStatus))
  {
    currentChildStatus = WTERMSIG(currentChildStatus);
    printf(", signal=%d\n", currentChildStatus);
    currentChildStatus += 128;
  }

  previous.endsWithNewLine = actual.endsWithNewLine;
  previous.writenTo = -1;
  status += currentChildStatus;
}


void freePat(pat_t *pat)
{
  free(pat->sep);
  if (pat->cmd_arr == NULL)
  {
    return;
  }
  for (int i = 0; i < pat->cmd_n; i++)
  {
    for (int j = 0; pat->cmd_arr[i][j]; j++)
    {
      free(pat->cmd_arr[i][j]);
    }
    free(pat->cmd_arr[i]);
  }
  free(pat->cmd_arr);
}


void freePipeArr(int ***pipeArr, int num_open_fds)
{
  for (int i = 0; i < num_open_fds; i++)
  {
    for (int j = 0; j < 2; j++)
    {
      free(pipeArr[i][j]);
    }
    free(pipeArr[i]);
  }
  free(pipeArr);
}


void freeAll(pat_t* pat, struct pollfd* p, int ***pipeArr)
{
  freePipeArr(pipeArr, pat->cmd_n);
  freePat(pat);
  if (p)
  {
    free(p);
  }
  free(separator);
}

void pexit(char *msg, int returnCode, pat_t *pat)
{
  fprintf(stderr, "%s", msg);
  freePat(pat);
  exit(returnCode);
}

// Initialiser le séparateur par défault
char *setDefaultSeparator()
{
  char def_sep = DEFAULT_SEP;
  char *sep = malloc(2 * sizeof(char));
  if (sep == NULL)
  {
    fprintf(stderr, "pat: Failed to allocate %zu bytes\n", (2 * sizeof(char)));
    exit(ERR_CODE);
  }

  *sep = '\0';
  strncat(sep, &def_sep, 1);
  return sep;
}

// Initialiser le séparateur entrée par l'utilisateur (s'il existe)
void setSeparator(pat_t *pat, char *sep)
{
  free(pat->sep);
  pat->sep = calloc(1, sizeof(sep));
  if (pat->sep == NULL)
  {
    fprintf(stderr, "pat: Failed to allocate %zu bytes\n", sizeof(sep));
    exit(ERR_CODE);
  }
  strncpy(pat->sep, sep, strsize(sep));
}

// Vérifier l'option 'c' dans 'src'
int checkOption(char c, char *src)
{
  if (src[0] == '-')
  {
    if (strsize(src) == 2 && src[1] == 's')
    {
      return 1;
    }
    return -1;
  }
  return 0;
}


/* ********************************************************
 * Verifier si les arguments entrée par l'utilisateur sont 
 * valides et enregistrer les dans pat_t.
 * Sinon afficher un message d'erreur et quitter le programme
 */

void checkArgs(pat_t *pat, int argc, char **argv)
{
  if (argc == 1)
  {
    pexit(USG_MSG, ERR_CODE, pat);
  }
  else if (argc > 1)
  {
    int check = checkOption('s', argv[1]);
    if (check == -1)
    {
      fprintf(stderr, "pat: %s: invalid option\n", argv[1]);
      pexit(USG_MSG, ERR_CODE, pat);
    }
    else if (check == 1)
    {
      if (argv[2] == NULL)
      {
        fprintf(stderr, "pat: separator not found\n");
        pexit(USG_MSG, ERR_CODE, pat);
      }
      if (argc < 4)
      {
        fprintf(stderr, "pat: Not enough arguments\n");
        pexit(USG_MSG, ERR_CODE, pat);
      }
      pat->flag_s++;
      setSeparator(pat, argv[2]);
    }

  }
}

// Trouver le nombre de commande et l'enregistrer dans pat_t. 
void setNumberOfCmd(int argc, char **argv, pat_t *pat)
{
  int start = 1;
  if (pat->flag_s) 
  {
    start = 3;
  }
  for (int i = start; i < argc; i++)
  {
    if (!strcmp(argv[i], pat->sep))
    {
      pat->cmd_n++;
    }
  }
}


/* ********************************************************
 * Trouver et retourner l'indexe du prochain séparateur.
 * (s'il existe)
 * 
 *  Paramètres:
 *    j     --> commancer la recherche à j + 1
 *    sep   --> utiliser ce séparateur
 *    argv  --> rechercher ici
 *    argc  --> rechercher jusqu'ici
 *
 *  Retours:
 *    i     --> index du prochain séparateurs
 *    argc  --> aucun séparateur trouver
 */

int indexOfNextSep(int argc, char **argv, int j, char *sep)
{
  for (int i = j + 1; i < argc; i++)
  {
    if (strncmp(argv[i - 1], "-s", 2) && !strcmp(argv[i], sep))
    {
      return i;
    }
  }
  return argc;
}


//Initialiser le tableau cmd_arr de pat_t.
void setCmd(pat_t *pat, int argc, char **argv)
{
  int tmp = 1;
  if (pat->flag_s) 
  {
    tmp = 3;
  }
  int index = 0;
  int j = 0;
  int z = 0;
  size_t size = pat->cmd_n * sizeof(char **);
  pat->cmd_arr = calloc(pat->cmd_n, sizeof(char **));
  if (pat->cmd_arr == NULL)
  {
    fprintf(stderr, "pat: Failed to allocate %zu bytes\n", size);
    free(pat->sep);
    exit(ERR_CODE);
  }

  for (int i = 0; i < pat->cmd_n; i++)
  {
    int nextIndex = indexOfNextSep(argc, argv, index, pat->sep);
    size = (nextIndex - index - 1) * sizeof(char *);
    pat->cmd_arr[i] = calloc((nextIndex - index - 1 + 4), sizeof(char *));
    if (pat->cmd_arr[i] == NULL)
    {
      fprintf(stderr, "pat: Failed to allocate %zu bytes\n", size);
      freePat(pat);
      exit(ERR_CODE);
    }
    for (j = index + 1; j < nextIndex; j++)
    {
      if (tmp == 3)
      {
        j += 1;
        tmp++;
        continue;
      }
      size = sizeof(argv[j]);
      pat->cmd_arr[i][z] = calloc(1, sizeof(argv[j]));
      if (pat->cmd_arr[i][z] == NULL)
      {
        fprintf(stderr, "pat: Failed to allocate %zu bytes\n", size);
        freePat(pat);
        exit(ERR_CODE);
      }
      strncpy(pat->cmd_arr[i][z], argv[j], strsize(argv[j]));
      z++;
    }
    pat->cmd_arr[i][z] = NULL;
    z = 0;
    index = j;
  }
}

/* ********************************************************
 * Initialiser 2 tubes pour chaque élément de cmd_arr.
 * Le premier pour l'entrée standard et le deuxième pour
 * la sortie standard.
 * 
 * exemple:  ./pat cat prog1 + ./prog2
 * --------
 * cmd_arr = [[cat, prog1, NULL], [./prog2, NULL]]
 * pipeArr = [[pipe_stdout1, pipe_stderr1], [pipe_stdout2, pipe_stderr2]]
 */

int ***setPipeArr(pat_t *pat)
{
  int ***pipeArr;
  size_t size = pat->cmd_n * sizeof(int **);
  pipeArr = calloc(pat->cmd_n, sizeof(int **));
  if (pipeArr == NULL)
  {
    fprintf(stderr, "pat: Failed to allocate %zu bytes\n", size);
    freePat(pat);
    exit(ERR_CODE);
  }

  for (int i = 0; i < pat->cmd_n; i++)
  {
    size = 2 * sizeof(int *);
    pipeArr[i] = calloc(2, sizeof(int *));
    if (pipeArr[i] == NULL)
    {
      fprintf(stderr, "pat: Failed to allocate %zu bytes\n", size);
      freeAll(pat, NULL, pipeArr);
      exit(ERR_CODE);
    }

    for (int j = 0; j < 2; j++)
    {
      size = 2 * sizeof(int);
      pipeArr[i][j] = calloc(2, sizeof(int));
      if (pipeArr[i][j] == NULL)
      {
        fprintf(stderr, "pat: Failed to allocate %zu bytes\n", size);
        freeAll(pat, NULL, pipeArr);
        exit(ERR_CODE);
      }

      if (pipe(pipeArr[i][j]) == -1)
      {
        fprintf(stderr, "pat: Failed to pipe\n");
        freeAll(pat, NULL, pipeArr);
        exit(ERR_CODE);
      }

    }
  }

  return pipeArr;
}

/* ********************************************************
 * Pour chaque élément de cmd_arr, faire un fork. 
 * 
 * Remplacer le descripteur de fichier de la sortie standard 
 * et de la sortie d'erreur pour chaque processus fils, par 
 * les descripteurs des tubes appropriés.
 * 
 * Executer les commandes.
 * 
 *  Paramètres:
 *    cmdArr    --> tableau contenant les commandes (cmd_arr de pat_t)
 *    pipeArr   --> tableau contenant les tubes
 *    n         --> nombre de commande
 */

int _fork(char ***cmdArr, int ***pipeArr, int n)
{
  pid_t pid;
  for (int j = 0; j < n; j++)
  {
    pid = fork();
    if(pid == -1)
    {
     fprintf(stderr, "pat: Failed to fork\n");
     return -1;
    }
    else if (pid == 0)
    {
      if(dup2(pipeArr[j][0][1], STDOUT_FIsizeO) == -1)
      {
        fprintf(stderr, "pat: Failed to clone fd: %d\n", STDOUT_FIsizeO);
        return -1;
      }
      if(dup2(pipeArr[j][1][1], STDERR_FIsizeO) == -1)
      {
        fprintf(stderr, "pat: Failed to clone fd: %d\n", STDERR_FIsizeO);
        return -1;
      }

      close(pipeArr[j][0][0]);
      close(pipeArr[j][0][1]);
      close(pipeArr[j][1][0]);
      close(pipeArr[j][1][1]);

      execvp(cmdArr[j][0], cmdArr[j]);
      perror(cmdArr[j][0]);
      exit(127);
    }
  }

  return 0;
}

// Initialiser struct pollfd, et fermer les tubes inutiles.
struct pollfd* setPoll(pat_t *pat, int ***pipeArr)
{
  size_t size = 2 * pat->cmd_n * sizeof(struct pollfd);
  struct pollfd *p = calloc(2 * pat->cmd_n, sizeof(struct pollfd));

  if (p == NULL)
  {
    fprintf(stderr, "pat: Failed to allocate %zu bytes\n", size);
    freeAll(pat, p, pipeArr);
    exit(ERR_CODE);
  }

  int i = 0;
  for (int j = 0; j < (2 * pat->cmd_n) - 1; j++)
  {
    p[j].fd = pipeArr[i][0][0];
    p[j].events = POLLIN | POLLHUP;
    p[j].revents = 0;

    p[j + 1].fd = pipeArr[i][1][0];
    p[j + 1].events = POLLIN | POLLHUP;
    p[j + 1].revents = 0;
    j++;

    close(pipeArr[i][0][1]);
    close(pipeArr[i][1][1]);
    i++;
  }
  return p;
}

/* ********************************************************
 * Trouver le numéro de la commande.
 *
 * i représente l'indexe d'un tube. Si l'indexe est pair, il
 * s'agit d'un tube qui lit de l'stdout. S'il est impair
 * alors c'est un tube qui lit de l'stderr.
 * 
 * Par exemple, si on a 2 commandes, on va avoir 4 tubes.
 * Le tableau pipeArr va contenir à l'indexe 0 et 1 les tubes
 * de la premières commande, et à l'indexe 2 et 3 les tubes
 * de la deuxième.
 * 
 * En faisant les opérations si-dessous on pourrait déterminer
 * le numéro de la commandes, en se basant sur l'indexe des
 * tubes.
 */

int getCmdNumber(int i)
{
  if(i % 2 == 0)
  {
    return (i + 2) / 2;
  }
  return (i + 1) / 2;
}

// Initialiser les ressources globals
int setGlobalRessources(char *sep, int cmd_n)
{
  separator = calloc(1, sizeof(sep));
  if (separator == NULL)
  {
    fprintf(stderr, "pat: Failed to allocate %zu bytes\n", sizeof(sep));
    return -1;
  }
  strncpy(separator, sep, strsize(sep));
  numberOfCmds = cmd_n;
  return 0;
}

/* ********************************************************
 * Démarrer le poll, lire et afficher le contenue de chaque 
 * tube. À chaque itération, actual et previous vont être
 * mis à jour, pour faire les bons affichages.
 */

void run(struct pollfd* p, pat_t *pat, int ***pipeArr)
{
  int nfds = 2 * pat->cmd_n;
  while (nfds > 0)
  {
    poll(p, nfds, -1);
    for (int i = 0; i < nfds; i++)
    {
      if (p[i].revents & (POLLIN | POLLHUP))
      {
        //update actual output_t
        actual.writenTo = (i % 2) + 1;
        actual.cameFrom = getCmdNumber(i);
        char buf[50];
        size_t size = read(p[i].fd, buf, 50);
        if(size == -1)
        {
          fprintf(stderr, "pat: Failed to read\n");
          freeAll(pat, p, pipeArr);
          exit(ERR_CODE);
        }
        else if (size == 0)
        {
          nfds--;
          continue;
        }

        if(canPrintSepWithNewLine())
        {
          if(actual.writenTo == 1)
          {
            printf("\n%s", separator);
            printSep("stdout", actual.cameFrom);
          }
          else if (actual.writenTo == 2)
          {
            printf("\n%s", separator);
            printSep("stderr", actual.cameFrom);
          }
        }
        else if(canPrintSepWithoutNewLine())
        {
          if(actual.writenTo == 1)
          {
            printSep("stdout", actual.cameFrom);
          }
          else if (actual.writenTo == 2)
          {
            printSep("stderr", actual.cameFrom);
          }
        }

        printf("%.*s", (int)size, buf);
        fflush(stdout);

        // update previous output_t
        previous.cameFrom = actual.cameFrom;

        if(buf[size - 1] != '\n')
        {
          previous.endsWithNewLine = 0;
        }
        else
        {
          previous.endsWithNewLine = 1;
        }
        previous.writenTo = actual.writenTo;
      }
    }
  }

  sleep(1);
}

int main(int argc, char *argv[])
{
  signal(SIGCHLD, handler);
  int ***pipeArr;
  struct pollfd *p;
  pat_t pat = {0, setDefaultSeparator(), 1, NULL};
  checkArgs(&pat, argc, argv);
  setNumberOfCmd(argc, argv, &pat);
  setCmd(&pat, argc, argv);
  pipeArr = setPipeArr(&pat);
  if (setGlobalRessources(pat.sep, pat.cmd_n) == -1)
  {
    freeAll(&pat, NULL, pipeArr);
    exit(ERR_CODE);
  }

  if (_fork(pat.cmd_arr, pipeArr, pat.cmd_n) == -1)
  {
    freeAll(&pat, NULL, pipeArr);
    exit(ERR_CODE);
  }

  p = setPoll(&pat, pipeArr);
  
  run(p, &pat, pipeArr);

  for (int j = 0; j < pat.cmd_n; j++)
  {
    close(pipeArr[j][0][0]);
    close(pipeArr[j][1][0]);
  }

  freeAll(&pat, p, pipeArr);
  return status;
}
