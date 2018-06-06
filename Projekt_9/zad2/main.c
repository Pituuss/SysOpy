#define _DEFAULT_SOURCE
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum customer_mode { GT, LT, EQ } customer_mode;

typedef enum run_mode { FULL, SIMPLE } run_mode;

int P;
int K;
int N;
FILE *source_file;
int L;
customer_mode mode_c;
run_mode mode_r;
int nk;
int completed;

char **cycle_buff;
sem_t *sem_set;
pthread_t *fac_threads;
pthread_t *cus_threads;

int const buff_size = 512;
int produce_to_index = 0;
int read_from_index = 0;

void init_params(FILE *fp);
void start();

void sig_hanlder(int signum) {
  for (int p = 0; p < P; p++) pthread_cancel(fac_threads[p]);
  for (int k = 0; k < K; k++) pthread_cancel(cus_threads[k]);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    perror("not enougth arguments");
    exit(1);
  }

  FILE *conf_file = fopen(argv[1], "r");

  init_params(conf_file);

  signal(SIGINT, sig_hanlder);
  if (nk > 0) signal(SIGALRM, sig_hanlder);

  cycle_buff = calloc(N, sizeof(char *));

  fac_threads = malloc(P * sizeof(pthread_t));
  cus_threads = malloc(K * sizeof(pthread_t));

  sem_set = malloc((N + 3) * sizeof(sem_t));
  for (int i = 0; i < N + 2; ++i) sem_init(&sem_set[i], 0, 1);
  sem_init(&sem_set[N + 2], 0, (unsigned int)N);

  start();

  for (int p = 0; p < P; ++p) pthread_join(fac_threads[p], NULL);
  completed = 1;
  for (int k = 0; k < K; ++k) pthread_join(cus_threads[k], NULL);

  fclose(conf_file);
  fclose(source_file);

  for (int j = 0; j < N + 4; ++j) {
    sem_destroy(&sem_set[j]);
  }
  free(sem_set);

  for (int i = 0; i < N; i++) {
    free(cycle_buff[i]);
  }
  free(cycle_buff);
}

void init_params(FILE *fp) {
  char buff[buff_size];
  fgets(buff, buff_size, fp);
  P = strtol(buff, NULL, 10);
  fgets(buff, buff_size, fp);
  K = strtol(buff, NULL, 10);
  fgets(buff, buff_size, fp);
  N = strtol(buff, NULL, 10);
  fgets(buff, buff_size, fp);

  source_file = fopen(strtok(buff, "\n"), "r");
  if (source_file == NULL) {
    printf("%s", buff);
    perror("failed to open the source_file\n");
    exit(3);
  }
  fgets(buff, buff_size, fp);
  L = strtol(buff, NULL, 10);
  fgets(buff, buff_size, fp);
  strtok(buff, "\n");
  if (strcmp(buff, "GT") == 0)
    mode_c = GT;
  else if (strcmp(buff, "LT") == 0)
    mode_c = LT;
  else
    mode_c = EQ;
  fgets(buff, buff_size, fp);
  if (strcmp(strtok(buff, "\n"), "FULL") == 0)
    mode_r = FULL;
  else
    mode_r = SIMPLE;

  fgets(buff, buff_size, fp);
  nk = strtol(buff, NULL, 10);

  completed = 0;

  printf(
      "completed\n P: %d\n K: %d\n N: %d\n L: %d\n customer_mode: %d\n "
      "run_mode: "
      "%d\n nk: %d\n completed: %d\n",
      P, K, N, L, mode_c, mode_r, nk, completed);
}

int cmp_length(int l) {
  int d = l - L;

  if (d < 0 && mode_c == LT) return 1;
  if (d > 0 && mode_c == GT) return 1;
  if (d == 0 && mode_c == EQ) return 1;

  return 0;
}

void *factory(void *pVoid) {
  int current_index;
  char line[LINE_MAX];
  while (fgets(line, LINE_MAX, source_file) != NULL) {
    if (mode_r == FULL) {
      fprintf(stderr, "Factory:%ld: taking file line\n", pthread_self());
    }
    sem_wait(&sem_set[N]);

    sem_wait(&sem_set[N + 2]);

    current_index = produce_to_index;
    if (mode_r == FULL) {
      fprintf(stderr, "Factory:%ld: taking buffer current_index (%d)\n",
              pthread_self(), current_index);
    }
    produce_to_index = (produce_to_index + 1) % N;

    sem_wait(&sem_set[current_index]);
    sem_post(&sem_set[N]);

    cycle_buff[current_index] = malloc((strlen(line) + 1) * sizeof(char));
    strcpy(cycle_buff[current_index], line);
    if (mode_r == FULL) {
      fprintf(stderr,
              "Factory:%ld: line copied to buffer at current_index (%d)\n",
              pthread_self(), current_index);
    }

    sem_post(&sem_set[current_index]);
  }
  if (mode_r == FULL)
    fprintf(stderr, "Factory[%ld]: Finished\n", pthread_self());
  return NULL;
}

void *customer(void *pVoid) {
  char *line;
  int current_index;
  while (1) {
    sem_wait(&sem_set[N + 1]);
    while (cycle_buff[read_from_index] == NULL) {
      sem_post(&sem_set[N + 1]);
      if (completed) {
        if (mode_r == FULL) {
          fprintf(stderr, "Customer:%ld: PEACE OUT \n", pthread_self());
        }
        return NULL;
      }
      sem_wait(&sem_set[N + 1]);
    }

    current_index = read_from_index;
    if (mode_r == FULL) {
      fprintf(stderr, "Customer:%ld: getting current_index to read:%d:\n",
              pthread_self(), current_index);
    }
    read_from_index = (read_from_index + 1) % N;

    sem_wait(&sem_set[current_index]);

    line = cycle_buff[current_index];
    cycle_buff[current_index] = NULL;

    if (mode_r == FULL) {
      fprintf(stderr,
              "Customer:%ld: reading line from buffer at current_index:%d:\n",
              pthread_self(), current_index);
    }

    sem_post(&sem_set[N + 2]);
    sem_post(&sem_set[N + 1]);
    sem_post(&sem_set[current_index]);

    if (cmp_length((int)strlen(line))) {
      fprintf(
          stderr,
          "Customer:%ld: got line: %s \t from buffer at current_index:%d:\n",
          pthread_self(), line, current_index);
    }
    free(line);
  }
}

void start() {
  for (int p = 0; p < P; ++p)
    pthread_create(&fac_threads[p], NULL, factory, NULL);
  for (int k = 0; k < K; ++k)
    pthread_create(&cus_threads[k], NULL, customer, NULL);
  if (nk > 0) alarm(nk);
}